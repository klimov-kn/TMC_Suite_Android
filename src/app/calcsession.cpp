/// @file calcsession.cpp
/// @brief CalcSession — один расчёт задачи. См. calcsession.h.
/// @see https://www.tamic.ru/docs/platforms/android.html#kernels

#include "calcsession.h"

#include "platform.h"
#include "taskstore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSet>

namespace {

// Наибольший объём записи тактов поля за расчёт (её копия уходит и в папку задачи).
constexpr qint64 kFrameBudget = 100LL * 1024 * 1024;
// Кадров на шаг: до 400 — у примеров 8–10 кадров на период волны, плавно; больше на
// телефоне не нужно (решётка xr: 0,44 МБ на кадр, предел — 100 МБ, около 230 кадров).
constexpr int kMaxFrames = 400;

QPair<qint64, qint64> stamp(const QFileInfo &fi)
{
    return { fi.size(), fi.lastModified().toMSecsSinceEpoch() };
}

} // namespace

CalcSession *CalcSession::instance()
{
    static CalcSession *s = new CalcSession;
    return s;
}

CalcSession *CalcSession::create(QQmlEngine *, QJSEngine *)
{
    CalcSession *s = instance();
    QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

CalcSession::CalcSession()
{
    QJSEngine::setObjectOwnership(&m_runner, QJSEngine::CppOwnership);
    connect(&m_runner, &KernelRunner::finished, this, &CalcSession::onKernelFinished);
    connect(&m_runner, &KernelRunner::progressChanged, this, &CalcSession::onProgress);
    connect(Platform::instance(), &Platform::stopRequested, this, &CalcSession::stop);
}

void CalcSession::setPhase(const QString &p)
{
    m_phase = p;
    emit stateChanged();
}

void CalcSession::fail(int code, const QString &message)
{
    m_runner.appendLog(message);
    Platform::log(message);
    m_code = code;
    Platform::serviceStop();
    TaskStore::instance()->recordRun(m_folder, m_task, m_mode, code, 0.0, {});
    setPhase(QStringLiteral("done"));
    emit finished(code);
}

bool CalcSession::start(const QString &folder, const QString &task, const QString &mode,
                        bool interactive)
{
    if (busy())
        return false;
    TaskStore *store = TaskStore::instance();
    m_folder = folder;
    m_task = task;
    m_mode = mode.toUpper() == QLatin1String("X") ? QStringLiteral("X") : QStringLiteral("H");
    m_code = -1;
    m_seconds = 0.0;
    m_results.clear();
    m_snapshot.clear();
    m_lastNotifyMs = 0;
    m_runner.clearLog();
    store->setMode(folder, task, m_mode);   // выбор запоминается для задачи

    if (interactive)
        Platform::requestNotificationPermission();
    Platform::serviceStart(tr("TMC Suite: %1 (%2)").arg(task, m_mode));

    // 1. Рабочая папка — с нуля.
    setPhase(QStringLiteral("copyIn"));
    m_workDir = TaskStore::workDirFor(folder, task, m_mode);
    QDir(m_workDir).removeRecursively();
    if (!QDir().mkpath(m_workDir)) {
        fail(KernelRunner::CalcError, tr("Не создать рабочую папку %1").arg(m_workDir));
        return false;
    }
    // Ядру — настоящий путь (на Android /data/user/0 -> /data/data): он попадает
    // в заголовки результатов, как и в Windows.
    m_workDir = QFileInfo(m_workDir).canonicalFilePath();

    // 2. Задание и включаемые файлы.
    m_inputs = store->inputsOf(folder, task);
    m_runner.appendLog(tr("Папка: %1").arg(store->folderTitle(folder)));
    m_runner.appendLog(tr("Входные файлы: %1").arg(m_inputs.join(QStringLiteral(", "))));
    for (const QString &f : std::as_const(m_inputs)) {
        const QString dst = QDir(m_workDir).filePath(f);
        if (!store->copyIn(folder, f, dst)) {
            fail(KernelRunner::TplError, tr("Не скопировать %1 в рабочую папку").arg(f));
            return false;
        }
        m_snapshot.insert(f.toLower(), stamp(QFileInfo(dst)));
    }

    // 3. Ядро.
    setPhase(QStringLiteral("running"));
    Platform::log(QStringLiteral("calc start: %1 mode=%2 workdir=%3").arg(task, m_mode, m_workDir));
    m_recorder.setBudget(m_frameBudgetMb > 0 ? qint64(m_frameBudgetMb) * 1024 * 1024 : kFrameBudget);
    m_recorder.setExpectedTicks(0);
    m_recorder.setRequireTicks(true);
    m_recorder.setMaxFrames(m_frameMax > 0 ? m_frameMax : kMaxFrames);
    m_recorder.start(m_workDir);
    if (!m_runner.start(m_mode, QDir(m_workDir).filePath(task), m_workDir)) {
        // KernelRunner уже сообщил finished -> onKernelFinished.
        return false;
    }
    return true;
}

void CalcSession::stop()
{
    if (m_phase == QLatin1String("running"))
        m_runner.cancel();
}

void CalcSession::onProgress()
{
    if (m_phase != QLatin1String("running"))
        return;
    // Тактов в шаге — запись поля берёт кадры равномерно по ним.
    m_recorder.setExpectedTicks(m_runner.tickMax());
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastNotifyMs < 1000)    // уведомление — не чаще раза в секунду
        return;
    m_lastNotifyMs = now;
    QString text;
    const QString stage = m_runner.stage();
    if (stage == QLatin1String("ticks"))
        text = tr("Шаг %1 из %2 · такт %3 из %4").arg(m_runner.step()).arg(m_runner.stepCount())
                   .arg(m_runner.tick()).arg(m_runner.tickMax());
    else if (stage == QLatin1String("prepr"))
        text = tr("Препроцессор разворачивает задание…");
    else if (stage == QLatin1String("post"))
        text = tr("Шаг %1 из %2 · обработка результатов…").arg(m_runner.step()).arg(m_runner.stepCount());
    else
        text = tr("Шаг %1 из %2 · подготовка сетки…").arg(m_runner.step()).arg(m_runner.stepCount());
    Platform::serviceUpdate(tr("TMC Suite: %1 (%2) — %3 %").arg(m_task, m_mode)
                                .arg(int(m_runner.progress())),
                            text, int(m_runner.progress()));
}

void CalcSession::onKernelFinished(int code)
{
    if (m_phase != QLatin1String("running"))
        return;
    m_code = code;
    m_seconds = double(m_runner.elapsedMs()) / 1000.0;
    m_recorder.stop();
    if (m_recorder.framesWritten() > 0)
        m_runner.appendLog(tr("Тактов поля записано: %1 (%2)").arg(m_recorder.framesWritten())
                               .arg(TaskStore::formatSize(m_recorder.bytesWritten())));
    if (!m_recorder.status().isEmpty())
        m_runner.appendLog(m_recorder.status());
    Platform::log(QStringLiteral("field frames: %1, bytes %2, ticks %3 %4").arg(m_recorder.framesWritten())
                      .arg(m_recorder.bytesWritten()).arg(m_runner.tickMax()).arg(m_recorder.status()));

    // 4. Результаты — обратно в папку задачи: всё новое и изменённое.
    setPhase(QStringLiteral("copyOut"));
    TaskStore *store = TaskStore::instance();
    const QFileInfoList produced = QDir(m_workDir).entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    int failed = 0;
    for (const QFileInfo &fi : produced) {
        const auto it = m_snapshot.constFind(fi.fileName().toLower());
        if (it != m_snapshot.constEnd() && it.value() == stamp(fi))
            continue;   // входной файл, ядро его не меняло
        if (fi.fileName().endsWith(QLatin1String(".tmp~")))
            continue;   // недописанный кадр поля (расчёт прерван, compat tmc_fopen)
        if (store->copyOut(fi.absoluteFilePath(), m_folder, fi.fileName()))
            m_results.append(fi.fileName());
        else
            ++failed;
    }
    m_runner.appendLog(tr("Результатов скопировано в папку задачи: %1").arg(m_results.size()));
    if (failed > 0)
        m_runner.appendLog(tr("Не скопировано: %1 (нет места или доступа к папке)").arg(failed));

    store->recordRun(m_folder, m_task, m_mode, code, m_seconds, m_results);
    Platform::serviceStop();
    Platform::log(QStringLiteral("calc done: %1 mode=%2 code=%3 seconds=%4 results=%5")
                      .arg(m_task, m_mode).arg(code).arg(m_seconds, 0, 'f', 2).arg(m_results.size()));
    setPhase(QStringLiteral("done"));
    emit finished(code);
}
