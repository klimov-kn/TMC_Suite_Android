/// @file autotest.cpp
/// @brief Autotest — режим самопроверки. См. autotest.h.
/// @see https://www.tamic.ru/docs/platforms/android.html#kernels

#include "autotest.h"

#include "calcsession.h"
#include "platform.h"
#include "taskstore.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QTimer>

QList<Autotest::Case> Autotest::allCases()
{
    return {
        { QStringLiteral("voln_h"),   QStringLiteral("voln"),       QStringLiteral("VOLN.TPL"),       QStringLiteral("H") },
        { QStringLiteral("voln_x"),   QStringLiteral("voln"),       QStringLiteral("VOLN.TPL"),       QStringLiteral("X") },
        { QStringLiteral("horn_h"),   QStringLiteral("horn"),       QStringLiteral("horn.tpl"),       QStringLiteral("H") },
        { QStringLiteral("horn_x"),   QStringLiteral("horn"),       QStringLiteral("horn.tpl"),       QStringLiteral("X") },
        { QStringLiteral("plasma_x"), QStringLiteral("plasma_nby"), QStringLiteral("plasma_nby.tpl"), QStringLiteral("X") },
    };
}

QList<Autotest::Case> Autotest::parse(const QString &spec)
{
    const QList<Case> all = allCases();
    if (spec.trimmed().compare(QLatin1String("all"), Qt::CaseInsensitive) == 0)
        return all;
    QList<Case> out;
    for (const QString &raw : spec.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString id = raw.trimmed().toLower();
        bool found = false;
        for (const Case &c : all) {
            if (c.id == id) {
                out.append(c);
                found = true;
            }
        }
        if (!found)
            return {};
    }
    return out;
}

Autotest::Autotest(const QList<Case> &cases, bool quitWhenDone, QObject *parent)
    : QObject(parent), m_cases(cases), m_quit(quitWhenDone)
{
    m_baseDir = QDir(Platform::externalDir()).filePath(QStringLiteral("autotest"));
}

void Autotest::line(const QString &text)
{
    m_report.append(text);
    Platform::log(QStringLiteral("AUTOTEST ") + text);
}

void Autotest::start()
{
    connect(CalcSession::instance(), &CalcSession::finished, this, &Autotest::onFinished);
    m_total.start();
    // Только папки своих случаев: результаты других случаев (и «последний расчёт» на
    // экране «Результаты») остаются.
    for (const Case &c : std::as_const(m_cases))
        QDir(QDir(m_baseDir).filePath(c.id)).removeRecursively();
    QDir().mkpath(m_baseDir);
    line(QStringLiteral("begin %1 cases=%2 version=%3 os=%4 cpu=%5")
             .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
             .arg(m_cases.size())
             .arg(QCoreApplication::applicationVersion(), QSysInfo::prettyProductName(),
                  QSysInfo::currentCpuArchitecture()));
    next();
}

void Autotest::next()
{
    ++m_index;
    if (m_index >= m_cases.size()) {
        finishAll();
        return;
    }
    const Case &c = m_cases.at(m_index);
    const QString dir = QDir(m_baseDir).filePath(c.id);
    if (!TaskStore::copyExampleTo(c.example, dir)) {
        line(QStringLiteral("case=%1 FAIL: example %2 not copied").arg(c.id, c.example));
        QTimer::singleShot(0, this, &Autotest::next);
        return;
    }
    line(QStringLiteral("case=%1 start mode=%2 tpl=%3 folder=%4").arg(c.id, c.mode, c.tpl, dir));
    if (!CalcSession::instance()->start(dir, c.tpl, c.mode, false)
        && CalcSession::instance()->phase() != QLatin1String("done")) {
        line(QStringLiteral("case=%1 FAIL: calculation not started").arg(c.id));
        QTimer::singleShot(0, this, &Autotest::next);
    }
}

void Autotest::onFinished(int code)
{
    if (m_index < 0 || m_index >= m_cases.size())
        return;
    const Case &c = m_cases.at(m_index);
    CalcSession *s = CalcSession::instance();
    const QString dir = QDir(m_baseDir).filePath(c.id);
    const bool ok = code == 0 && !s->resultFiles().isEmpty();
    if (ok)
        ++m_passed;
    line(QStringLiteral("case=%1 %2 code=%3 seconds=%4 inputs=%5 results=%6")
             .arg(c.id, ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
             .arg(code).arg(s->seconds(), 0, 'f', 2)
             .arg(s->inputFiles().size()).arg(s->resultFiles().size()));
    line(QStringLiteral("case=%1 workdir=%2").arg(c.id, s->workDir()));
    for (const QString &f : s->resultFiles()) {
        QFile file(QDir(dir).filePath(f));
        QByteArray md5;
        if (file.open(QIODevice::ReadOnly)) {
            QCryptographicHash h(QCryptographicHash::Md5);
            h.addData(&file);
            md5 = h.result().toHex();
        }
        line(QStringLiteral("case=%1 file=%2 size=%3 md5=%4")
                 .arg(c.id, f).arg(QFileInfo(file).size()).arg(QString::fromLatin1(md5)));
    }
    // Журнал ядра — для разбора, рядом с результатами (не в папке задачи).
    QFile log(QDir(m_baseDir).filePath(c.id + QStringLiteral(".log")));
    if (log.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        log.write(s->runner()->logText().toUtf8() + '\n');
    QTimer::singleShot(200, this, &Autotest::next);
}

void Autotest::finishAll()
{
    const bool ok = m_passed == m_cases.size();
    line(QStringLiteral("summary %1 passed=%2/%3 seconds=%4")
             .arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
             .arg(m_passed).arg(m_cases.size())
             .arg(double(m_total.elapsed()) / 1000.0, 0, 'f', 1));
    const QByteArray text = m_report.join(QLatin1Char('\n')).toUtf8() + '\n';
    const QString internal = QDir(Platform::dataDir()).filePath(QStringLiteral("autotest"));
    QDir().mkpath(internal);
    for (const QString &path : { QDir(internal).filePath(QStringLiteral("report.txt")),
                                 QDir(m_baseDir).filePath(QStringLiteral("report.txt")) }) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(text);
    }
    emit done(ok);
    if (m_quit)
        QCoreApplication::exit(ok ? 0 : 1);
}
