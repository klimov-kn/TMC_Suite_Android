/// @file calcsession.h
/// @brief CalcSession (в QML — Calc) — один расчёт задачи от начала до конца:
///
///   1. рабочая папка <данные>/calc/<задача>_<H|X>_<ключ> очищается;
///   2. задание и файлы, на которые оно ссылается, копируются туда из папки задачи;
///   3. ядро считает через KernelRunner (на Android — под службой переднего плана, П3);
///   4. новые и изменённые файлы копируются обратно в папку задачи;
///   5. итог (код, время, список результатов) запоминается для экранов.
/// Режим самопроверки (Autotest) идёт по тому же пути.
/// @see https://www.tamic.ru/docs/platforms/android.html#kernels

#pragma once

#include "fieldrecorder.h"
#include "kernelrunner.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class CalcSession : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Calc)
    QML_SINGLETON

    Q_PROPERTY(KernelRunner *runner READ runner CONSTANT)
    Q_PROPERTY(QString folder READ folder NOTIFY stateChanged)
    Q_PROPERTY(QString task READ task NOTIFY stateChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int resultCode READ resultCode NOTIFY stateChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY stateChanged)
    Q_PROPERTY(QString workDir READ workDir NOTIFY stateChanged)

public:
    static CalcSession *instance();
    static CalcSession *create(QQmlEngine *, QJSEngine *);

    // interactive — запуск человеком (спросить разрешение на уведомления).
    Q_INVOKABLE bool start(const QString &folder, const QString &task, const QString &mode,
                           bool interactive = true);
    Q_INVOKABLE void stop();
    // Пределы записи тактов поля (МБ, кадров) — для проверочного запуска (съёмка видео);
    // 0 — по умолчанию.
    void setFrameLimits(int budgetMb, int maxFrames) { m_frameBudgetMb = budgetMb; m_frameMax = maxFrames; }

    KernelRunner *runner() { return &m_runner; }
    QString folder() const { return m_folder; }
    QString task() const { return m_task; }
    QString mode() const { return m_mode; }
    // idle · copyIn · running · copyOut · done
    QString phase() const { return m_phase; }
    bool busy() const { return m_phase == QLatin1String("copyIn") || m_phase == QLatin1String("running")
                               || m_phase == QLatin1String("copyOut"); }
    int resultCode() const { return m_code; }
    int resultCount() const { return int(m_results.size()); }
    QStringList resultFiles() const { return m_results; }
    QStringList inputFiles() const { return m_inputs; }
    QString workDir() const { return m_workDir; }
    double seconds() const { return m_seconds; }

signals:
    void stateChanged();
    void finished(int code);

private:
    CalcSession();
    void setPhase(const QString &p);
    void onKernelFinished(int code);
    void onProgress();
    void fail(int code, const QString &message);

    KernelRunner m_runner;
    // Такты поля (FIELDS) пишутся в <имя>.ex.frames рядом с .ex — для проигрывания
    // на экране «Поле» вперёд и назад.
    fieldview::FieldRecorder m_recorder;
    QString m_folder, m_task, m_mode, m_phase = QStringLiteral("idle"), m_workDir;
    QStringList m_inputs, m_results;
    QHash<QString, QPair<qint64, qint64>> m_snapshot;   // имя -> (размер, время) после копирования
    int m_code = -1;
    double m_seconds = 0.0;
    int m_frameBudgetMb = 0;
    int m_frameMax = 0;
    qint64 m_lastNotifyMs = 0;
};
