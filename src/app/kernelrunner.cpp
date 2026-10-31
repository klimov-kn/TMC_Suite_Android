// KernelRunner — запуск ядра процессом (Android, настольная сборка). См. kernelrunner.h.
//
// Готового класса запуска ядра процессом в версиях для Linux и macOS нет: там
// ядро — окно MFC-совместимого слоя, а внешние программы запускаются эмуляцией
// CreateProcess (fork/execvp, src/compat/tmc_mfc_doc.cpp). Отсюда взято только
// поведение: ошибка запуска сообщается сразу (а не «успехом fork»), рабочий
// каталог — каталог задания. Строки хода — те же, что ядро выводит в окно
// состояния Windows-версии (PutStatistics), их формат не меняется.

#include "kernelrunner.h"

#include "platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>

namespace {

constexpr int kMaxLogLines = 400;      // журнал на экране: последние строки
constexpr int kUiIntervalMs = 200;     // ≤ 5 обновлений в секунду
constexpr int kKillDelayMs = 2000;     // SIGTERM -> SIGKILL

// Убрать префикс «[tmc_rth] » у строк stdout ядра.
QString stripPrefix(const QString &line)
{
    if (line.startsWith(QLatin1Char('['))) {
        const qsizetype end = line.indexOf(QLatin1String("] "));
        if (end > 0)
            return line.mid(end + 2);
    }
    return line;
}

} // namespace

// ---------------------------------------------------------------- KernelProgress

bool KernelProgress::parseLine(const QString &rawLine)
{
    static const QRegularExpression reSteps(QStringLiteral("^Steps:\\s*(\\d+)"));
    static const QRegularExpression reStep(QStringLiteral("^Step\\s+(\\d+)\\s+of\\s+(\\d+):\\s*(read|start|done)"));
    static const QRegularExpression reTmax(QStringLiteral("^nTmax\\s+(\\d+)"));
    static const QRegularExpression reTick(QStringLiteral("^nT:(\\d+)(?:;Time:([^;]*))?"));
    static const QRegularExpression reResult(QStringLiteral("^RESULT\\s+(\\d+):"));

    const QString line = stripPrefix(rawLine.trimmed());
    QRegularExpressionMatch m;
    if ((m = reSteps.match(line)).hasMatch()) {
        stepCount = m.captured(1).toInt();
        return true;
    }
    if ((m = reStep.match(line)).hasMatch()) {
        step = m.captured(1).toInt();
        stepCount = std::max(stepCount, m.captured(2).toInt());
        // stdout («Step …») и stderr («nTmax», «nT:…») — два канала, и QProcess может
        // прочитать stderr раньше: «start» приходит уже ПОСЛЕ «nTmax». Поэтому такты шага
        // обнуляются по «read» (он задолго до тактов), а «start» их не трогает — иначе
        // число тактов терялось, проценты стояли на нуле, запись поля брала один кадр.
        const QString what = m.captured(3);
        if (what == QLatin1String("done")) {
            stepsDone = step;
            tick = tickMax;
            reading = false;
        } else if (what == QLatin1String("read")) {
            reading = true;
            tick = 0;
            tickMax = 0;
            modelTime.clear();
        } else {
            reading = false;
        }
        return true;
    }
    if ((m = reTmax.match(line)).hasMatch()) {
        tickMax = m.captured(1).toInt();
        tick = 0;
        reading = false;
        return true;
    }
    if ((m = reTick.match(line)).hasMatch()) {
        tick = m.captured(1).toInt();
        if (m.hasCaptured(2) && !m.captured(2).isEmpty())
            modelTime = m.captured(2).trimmed();
        return true;
    }
    if ((m = reResult.match(line)).hasMatch()) {
        resultCode = m.captured(1).toInt();
        return true;
    }
    return false;
}

QString KernelProgress::stage() const
{
    if (resultCode >= 0)
        return QStringLiteral("end");
    if (stepCount <= 0)
        return QStringLiteral("prepr");
    if (reading || step <= stepsDone)
        return QStringLiteral("read");
    if (tickMax <= 0)
        return QStringLiteral("grid");
    if (tick >= tickMax)
        return QStringLiteral("post");
    return QStringLiteral("ticks");
}

double KernelProgress::percent() const
{
    if (stepCount <= 0)
        return 0.0;
    double part = 0.0;
    if (step > stepsDone && tickMax > 0)
        part = std::clamp(double(tick) / double(tickMax), 0.0, 1.0);
    const double p = (double(stepsDone) + part) / double(stepCount) * 100.0;
    return std::clamp(p, 0.0, 100.0);
}

// ---------------------------------------------------------------- KernelRunner

KernelRunner::KernelRunner(QObject *parent)
    : QObject(parent)
{
    m_uiTimer.setInterval(kUiIntervalMs);
    connect(&m_uiTimer, &QTimer::timeout, this, &KernelRunner::flushUi);
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(kKillDelayMs);
    connect(&m_killTimer, &QTimer::timeout, this, [this]() {
        if (m_proc && m_proc->state() != QProcess::NotRunning)
            m_proc->kill();
    });
}

KernelRunner::~KernelRunner()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(3000);
    }
}

QString KernelRunner::kernelPath(const QString &mode)
{
    const bool x = mode.compare(QLatin1String("X"), Qt::CaseInsensitive) == 0;
#if defined(Q_OS_ANDROID)
    return Platform::nativeLibraryDir() + (x ? QStringLiteral("/libtmc_rtx.so")
                                             : QStringLiteral("/libtmc_rth.so"));
#elif defined(Q_OS_WIN)
    return QCoreApplication::applicationDirPath() + (x ? QStringLiteral("/tmc_rtx.exe")
                                                       : QStringLiteral("/tmc_rth.exe"));
#else
    return QCoreApplication::applicationDirPath() + (x ? QStringLiteral("/tmc_rtx")
                                                       : QStringLiteral("/tmc_rth"));
#endif
}

qint64 KernelRunner::elapsedMs() const
{
    return m_running ? m_clock.elapsed() : m_lastElapsed;
}

void KernelRunner::appendLog(const QString &line)
{
    m_log.append(line);
    if (m_log.size() > kMaxLogLines)
        m_log.erase(m_log.begin(), m_log.begin() + (m_log.size() - kMaxLogLines));
    m_logDirty = true;
    if (!m_running)
        flushUi();
}

void KernelRunner::clearLog()
{
    m_log.clear();
    m_logDirty = true;
    flushUi();
}

bool KernelRunner::start(const QString &mode, const QString &tplPath, const QString &workDir)
{
    if (m_running) {
        appendLog(tr("Расчёт уже идёт"));
        return false;
    }
    m_progress = KernelProgress();
    m_stage.clear();
    m_stageStartMs = 0;
    m_exitCode = -1;
    m_cancelled = false;
    m_outBuf.clear();
    m_errBuf.clear();
    m_mode = mode.toUpper();
    m_lastElapsed = 0;

    const QString exe = kernelPath(m_mode);
    if (!QFileInfo::exists(exe)) {
        appendLog(tr("Нет программы ядра: %1").arg(QDir::toNativeSeparators(exe)));
        m_exitCode = CalcError;
        emit runningChanged();
        emit finished(m_exitCode);
        return false;
    }

    delete m_proc;
    m_proc = new QProcess(this);
    m_proc->setProgram(exe);
    m_proc->setArguments({ tplPath });
    m_proc->setWorkingDirectory(workDir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TMC_KERNEL_TRACE"), QStringLiteral("1"));   // такты в stderr
    m_proc->setProcessEnvironment(env);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &KernelRunner::readStdout);
    connect(m_proc, &QProcess::readyReadStandardError, this, &KernelRunner::readStderr);
    connect(m_proc, &QProcess::finished, this, &KernelRunner::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, &KernelRunner::onError);

    appendLog(tr("Ядро %1: %2").arg(m_mode, QFileInfo(exe).fileName()));
    m_running = true;
    m_clock.start();
    m_uiTimer.start();
    emit runningChanged();
    m_proc->start(QIODevice::ReadOnly);
    return true;
}

void KernelRunner::cancel()
{
    if (!m_running || !m_proc)
        return;
    m_cancelled = true;
    appendLog(tr("Остановка расчёта…"));
#ifdef Q_OS_WIN
    m_proc->kill();
#else
    m_proc->terminate();      // SIGTERM
    m_killTimer.start();      // через 2 с — SIGKILL
#endif
}

void KernelRunner::readStdout()
{
    m_outBuf += m_proc->readAllStandardOutput();
    qsizetype nl;
    while ((nl = m_outBuf.indexOf('\n')) >= 0) {
        const QByteArray raw = m_outBuf.left(nl);
        m_outBuf.remove(0, nl + 1);
        handleLine(QString::fromLocal8Bit(raw).trimmed(), true);
    }
}

void KernelRunner::readStderr()
{
    m_errBuf += m_proc->readAllStandardError();
    qsizetype nl;
    while ((nl = m_errBuf.indexOf('\n')) >= 0) {
        const QByteArray raw = m_errBuf.left(nl);
        m_errBuf.remove(0, nl + 1);
        handleLine(QString::fromLocal8Bit(raw).trimmed(), false);
    }
}

void KernelRunner::handleLine(const QString &line, bool fromStdout)
{
    if (line.isEmpty())
        return;
    if (m_progress.parseLine(line))
        m_progressDirty = true;
    // В журнал — строки хода ядра (stdout); из трассировки окна (stderr) — только
    // сообщения об ошибках: такты идут в полосу хода, а не в журнал.
    // «Step i of N: read» — только для стадии на экране; «File: …» — путь во внутренней
    // рабочей папке, человеку он ничего не говорит (папка и входные файлы — в журнале
    // выше). Обе строки в журнал не идут.
    static const QRegularExpression reHidden(
        QStringLiteral("^(Step\\s+\\d+\\s+of\\s+\\d+:\\s*read|File:\\s)"));
    if (fromStdout && !reHidden.match(stripPrefix(line.trimmed())).hasMatch())
        appendLog(stripPrefix(line));
    else if (line.contains(QLatin1String("Error"), Qt::CaseInsensitive)
             && !line.contains(QLatin1String("Error:no"), Qt::CaseInsensitive))
        appendLog(line);    // «Step:1;Error:no» — строка окна без ошибки, её не показываем
}

void KernelRunner::onError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        appendLog(tr("Ядро не запустилось: %1").arg(m_proc->errorString()));
        finish(CalcError);
    }
}

void KernelRunner::onFinished(int exitCode, QProcess::ExitStatus status)
{
    // Хвосты без перевода строки.
    if (!m_outBuf.isEmpty()) { m_outBuf += '\n'; readStdout(); }
    if (!m_errBuf.isEmpty()) { m_errBuf += '\n'; readStderr(); }

    int code = exitCode;
    if (m_cancelled) {
        code = Stopped;
        appendLog(tr("RESULT 3: остановлено — расчёт не закончен"));
    } else if (status == QProcess::CrashExit) {
        code = CalcError;
        appendLog(tr("Ядро завершилось аварийно"));
    } else if (code < Ok || code > Stopped) {
        appendLog(tr("Ядро вернуло неизвестный код %1").arg(code));
        code = CalcError;
    }
    finish(code);
}

void KernelRunner::finish(int code)
{
    if (!m_running)
        return;
    m_killTimer.stop();
    m_uiTimer.stop();
    m_lastElapsed = m_clock.elapsed();
    m_running = false;
    m_exitCode = code;
    m_progressDirty = true;
    flushUi();
    emit runningChanged();
    emit finished(code);
}

qint64 KernelRunner::stageMs() const
{
    return std::max<qint64>(0, elapsedMs() - m_stageStartMs);
}

void KernelRunner::flushUi()
{
    const QString st = m_progress.stage();
    if (st != m_stage) {
        m_stage = st;
        m_stageStartMs = elapsedMs();
    }
    if (m_running || m_progressDirty)
        emit progressChanged();     // время идёт и без новых строк
    m_progressDirty = false;
    if (m_logDirty) {
        m_logDirty = false;
        emit logChanged();
    }
}
