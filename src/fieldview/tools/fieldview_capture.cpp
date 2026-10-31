// fieldview_capture — настольная проверка FieldRecorder и подготовка тестовых серий:
// запускает ядро в папке задания и пишет такты поля в <имя>.ex.frames.
//
//   fieldview_capture <tmc_rth|tmc_rtx> <задание.tpl> [--interval мс] [--min-step такты]
//
// Код возврата — код ядра; 4 — ни одного такта не записано.
#include "fieldrecorder.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser p;
    p.addHelpOption();
    const QCommandLineOption intervalOpt(QStringLiteral("interval"), QStringLiteral("Опрос, мс"),
                                         QStringLiteral("ms"), QStringLiteral("20"));
    const QCommandLineOption stepOpt(QStringLiteral("min-step"), QStringLiteral("Шаг тактов"),
                                     QStringLiteral("n"), QStringLiteral("0"));
    p.addOptions({intervalOpt, stepOpt});
    p.addPositionalArgument(QStringLiteral("kernel"), QStringLiteral("tmc_rth / tmc_rtx"));
    p.addPositionalArgument(QStringLiteral("tpl"), QStringLiteral("задание"));
    p.process(app);
    if (p.positionalArguments().size() != 2)
        p.showHelp(2);

    const QString kernel = QFileInfo(p.positionalArguments().at(0)).absoluteFilePath();
    const QFileInfo tpl(p.positionalArguments().at(1));
    const QString dir = tpl.absolutePath();

    fieldview::FieldRecorder rec;
    rec.setInterval(p.value(intervalOpt).toInt());
    rec.setMinTactStep(p.value(stepOpt).toInt());
    rec.start(dir);

    QProcess proc;
    proc.setWorkingDirectory(dir);
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    int rc = -1;
    QObject::connect(&proc, &QProcess::finished, &app, [&](int code, QProcess::ExitStatus) {
        rc = code;
        rec.stop();
        app.quit();
    });
    proc.start(kernel, {tpl.fileName()});
    if (!proc.waitForStarted()) {
        std::fprintf(stderr, "не удалось запустить %s\n", qPrintable(kernel));
        return 3;
    }
    app.exec();
    std::fprintf(stderr, "[capture] тактов записано: %d, байт: %lld %s\n", rec.framesWritten(),
                 static_cast<long long>(rec.bytesWritten()), qPrintable(rec.status()));
    if (rec.framesWritten() == 0)
        return 4;
    return rc;
}
