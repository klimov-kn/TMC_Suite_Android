#include "taskfiles.h"

#include "cp1251.h"
#include "emitter.h"
#include "sigfile.h"

#include <QDir>
#include <QFile>

namespace tpl {

namespace {

/// Имена файлов одного задания конвейера.
QStringList chunkFileNames(const TaskChunk &c)
{
    QStringList out;
    out << c.name + QStringLiteral(".tpl");
    if (!c.stepHName.isEmpty())
        out << c.stepHName;
    return out;
}

/// Имена пакетных файлов. Оба сразу: папку носят между системами.
QStringList scriptNames(const Project &p)
{
    return {p.name + QStringLiteral("_run.bat"), p.name + QStringLiteral("_run.sh")};
}

} // namespace

QString runFlags(const Project &p)
{
    // `/Arb` — автозапуск с пакетным режимом (окно само закрывается).
    // `/Time` — отчёт времени расчёта рядом с заданием. В ЯЗЫКЕ задания такого ключа
    // нет и выдумывать его нельзя: это ключ командной строки ядра, поэтому он живёт
    // здесь, а не в #PARM. Старые сборки ядра ключ молча игнорируют.
    return p.output.timingReport ? QStringLiteral("/Arb /Time") : QStringLiteral("/Arb");
}

QStringList taskFileNames(const Project &p, const QMap<QString, QByteArray> &companions)
{
    QStringList out;
    const QVector<TaskChunk> chunks = emitChunks(p);
    for (const TaskChunk &c : chunks) {
        const QStringList names = chunkFileNames(c);
        out += names;
    }
    if (chunks.size() > 1) {
        const QStringList names = scriptNames(p);
        out += names;
    }
    // Файлы падающей волны генерируем мы сами — они появятся так же, как .tpl.
    const QMap<QString, QByteArray> sigs = signalCompanions(p);
    for (auto it = sigs.constBegin(); it != sigs.constEnd(); ++it)
        out << it.key();
    for (auto it = companions.constBegin(); it != companions.constEnd(); ++it)
        out << it.key();
    return out;
}

QString runBat(const Project &p, const QVector<TaskChunk> &chunks)
{
    const QString exe = p.kernel == Kernel::X ? QStringLiteral("tmc_rtx.exe")
                                              : QStringLiteral("tmc_rth.exe");
    QStringList L;
    L << QStringLiteral("@echo off");
    L << QStringLiteral("rem Прогон заданий конвейера по очереди.");
    L << QStringLiteral("rem Сгенерировано TPL Designer. Частот всего %1, заданий %2.")
             .arg(p.freqs.size()).arg(chunks.size());
    L << QStringLiteral("rem");
    L << QStringLiteral("rem Положите ядро в PATH либо впишите полный путь в KERNEL.");
    L << QStringLiteral("rem Файлы .s удаляются ПЕРЕД прогоном: ядро их дописывает, а не");
    L << QStringLiteral("rem перезаписывает, и от старых данных результат молча испортится.");
    L << QStringLiteral("rem");
    L << QStringLiteral("rem Задания независимы, поэтому их можно считать и одновременно:");
    L << QStringLiteral("rem   for %%f in (*.tpl) do start \"\" %%KERNEL%% \"%%f\" %1")
             .arg(runFlags(p));
    L << QString();
    L << QStringLiteral("set KERNEL=%1").arg(exe);
    L << QString();
    for (const TaskChunk &c : chunks) {
        L << QStringLiteral("echo === %1 (%2...%3 ГГц) ===")
                 .arg(c.name, formatCompact(c.freqs.first()), formatCompact(c.freqs.last()));
        // Имена .s у каждой части свои — их дал emitChunks.
        for (const QString &s : c.sMatrixFiles)
            L << QStringLiteral("if exist \"%1.s\" del \"%1.s\"").arg(s);
        L << QStringLiteral("%KERNEL% \"%1.tpl\" %2").arg(c.name, runFlags(p));
    }
    L << QString();
    L << QStringLiteral("echo Готово.");
    return L.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString runSh(const Project &p, const QVector<TaskChunk> &chunks)
{
    const QString exe = p.kernel == Kernel::X ? QStringLiteral("planarrt_x")
                                              : QStringLiteral("planarrt_h");
    const QString app = p.kernel == Kernel::X ? QStringLiteral("PlanarRT_X.app")
                                              : QStringLiteral("PlanarRT_H.app");
    QStringList L;
    L << QStringLiteral("#!/bin/sh");
    L << QStringLiteral("# Прогон заданий конвейера по очереди.");
    L << QStringLiteral("# Сгенерировано TPL Designer. Частот всего %1, заданий %2.")
             .arg(p.freqs.size()).arg(chunks.size());
    L << QStringLiteral("#");
    L << QStringLiteral("# Linux: ядро называется %1.").arg(exe);
    L << QStringLiteral("# macOS: программа лежит в пакете, запуск другой --");
    L << QStringLiteral("#   KERNEL=\"open -a %1 --args\"").arg(app);
    L << QStringLiteral("#");
    L << QStringLiteral("# Файлы .s удаляются ПЕРЕД прогоном: ядро их дописывает, а не");
    L << QStringLiteral("# перезаписывает, и от старых данных результат молча испортится.");
    L << QString();
    L << QStringLiteral("set -e");
    L << QStringLiteral("KERNEL=${KERNEL:-%1}").arg(exe);
    L << QStringLiteral("FLAGS=\"%1\"").arg(runFlags(p));
    L << QString();
    for (const TaskChunk &c : chunks) {
        L << QStringLiteral("echo \"=== %1 (%2...%3 GHz) ===\"")
                 .arg(c.name, formatCompact(c.freqs.first()), formatCompact(c.freqs.last()));
        for (const QString &s : c.sMatrixFiles)
            L << QStringLiteral("rm -f \"%1.s\"").arg(s);
        L << QStringLiteral("$KERNEL \"%1.tpl\" $FLAGS").arg(c.name);
    }
    L << QString();
    L << QStringLiteral("echo \"Done.\"");
    return L.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

WriteResult writeTask(const QString &dir, const Project &p,
                      const QMap<QString, QByteArray> &companions)
{
    WriteResult r;
    QDir().mkpath(dir);
    const auto put = [&](const QString &fileName, const QString &text) {
        bool l = false;
        if (!writeCp1251File(QDir(dir).filePath(fileName), text, &r.error, &l))
            return false;
        r.lossy |= l;
        r.written << fileName;
        return true;
    };

    // Конвейер и обычное задание идут одной дорогой: без деления chunks состоит из
    // одного элемента, и ветвиться не приходится.
    const QVector<TaskChunk> chunks = emitChunks(p);
    for (const TaskChunk &c : chunks) {
        if (!put(c.name + QStringLiteral(".tpl"), c.tpl))
            return r;
        if (!c.stepHName.isEmpty() && !put(c.stepHName, c.stepH))
            return r;
    }
    if (chunks.size() > 1) {
        const QStringList names = scriptNames(p);
        if (!put(names.at(0), runBat(p, chunks)) || !put(names.at(1), runSh(p, chunks)))
            return r;
    }

    // Файлы падающей волны: генерируются из задания, поэтому пишутся здесь, а не
    // приходят снаружи. Иначе рано или поздно один из путей записи их забыл бы.
    bool sigLossy = false;
    const QMap<QString, QByteArray> sigs = signalCompanions(p, &sigLossy);
    r.lossy |= sigLossy;

    QMap<QString, QByteArray> all = sigs;
    for (auto it = companions.constBegin(); it != companions.constEnd(); ++it)
        all.insert(it.key(), it.value());

    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        QFile f(QDir(dir).filePath(it.key()));
        if (!f.open(QIODevice::WriteOnly) || f.write(it.value()) != it.value().size()) {
            r.error = QStringLiteral("не удалось записать %1").arg(f.fileName());
            return r;
        }
        r.written << it.key();
    }
    r.ok = true;
    return r;
}

} // namespace tpl
