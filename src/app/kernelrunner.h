/// @file kernelrunner.h
/// @brief KernelRunner — общий интерфейс запуска счётного ядра.
///
/// Экраны видят только этот класс: старт, ход в процентах, шаг и такт, время,
/// журнал, код возврата и отмену. Как именно работает ядро, им не важно:
///   * Android и настольная сборка — отдельный процесс (QProcess): ядро —
///     программа tmc_rth / tmc_rtx (на Android — libtmc_rth.so / libtmc_rtx.so из
///     каталога нативных библиотек приложения);
///
/// Протокол ядра (src/kernels/cli/tmc_kernel_main.cpp и tmc_kernel_console.cpp):
///   stdout — строки «[tmc_rth] …»: «Steps: N», «Step i of N: read|start|done»,
///            «ERROR …», «RESULT k: …»;
///   stderr — при TMC_KERNEL_TRACE=1 строки окна состояния Windows-версии
///            (PutStatistics/PutTrace): «nTmax M» в начале шага и «nT:t;Time:…»
///            каждые 50 тактов (kernels/PlanarRT_H/TmcRTHRectNode.cpp, RunStep).
/// Код возврата: 0 — все шаги посчитаны, 1 — ошибка в .tpl, 2 — ошибка расчёта,
/// 3 — остановлено пользователем (выставляет KernelRunner).
/// @see https://www.tamic.ru/docs/platforms/android.html#kernels

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Разбор строк хода расчёта. Отдельно от процесса — чтобы проверять без ядра.
struct KernelProgress
{
    int stepCount = 0;   // «Steps: N»
    int step = 0;        // текущий шаг, 1..N (0 — ещё не начат)
    int stepsDone = 0;   // «Step i of N: done»
    int tickMax = 0;     // «nTmax M» — тактов в текущем шаге
    int tick = 0;        // «nT:t;…» — пройдено тактов в текущем шаге
    QString modelTime;   // модельное время из «nT:t;Time:0.12ns»
    int resultCode = -1; // «RESULT k: …»
    bool reading = false; // «Step i of N: read» — шаг читается, «start» ещё не было

    // true — строка изменила ход расчёта.
    bool parseLine(const QString &line);
    // Общий ход 0..100: (готовые шаги + доля тактов текущего) / число шагов.
    double percent() const;
    // Стадия — чтобы долгая подготовка не выглядела зависанием (проценты стоят):
    //   prepr — препроцессор разворачивает задание (#define, #include, шаги);
    //   read  — чтение шага: топология, ε и плазма на сетке;
    //   grid  — сетка готовится к счёту (до «nTmax»);
    //   ticks — идут такты;
    //   post  — такты кончились, обработка (S-матрица, ДН, запись файлов);
    //   end   — ядро сообщило итог.
    QString stage() const;
};

class KernelRunner : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("KernelRunner принадлежит Calc")

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int step READ step NOTIFY progressChanged)
    Q_PROPERTY(int stepCount READ stepCount NOTIFY progressChanged)
    Q_PROPERTY(int tick READ tick NOTIFY progressChanged)
    Q_PROPERTY(int tickMax READ tickMax NOTIFY progressChanged)
    Q_PROPERTY(QString modelTime READ modelTime NOTIFY progressChanged)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY progressChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY progressChanged)
    Q_PROPERTY(qint64 stageMs READ stageMs NOTIFY progressChanged)   // сколько идёт стадия
    Q_PROPERTY(QString logText READ logText NOTIFY logChanged)
    Q_PROPERTY(int exitCode READ exitCode NOTIFY runningChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY runningChanged)

public:
    enum Code { Ok = 0, TplError = 1, CalcError = 2, Stopped = 3 };
    Q_ENUM(Code)

    explicit KernelRunner(QObject *parent = nullptr);
    ~KernelRunner() override;

    // Полный путь к программе ядра "H" или "X" на этой платформе.
    static QString kernelPath(const QString &mode);

    // Запуск ядра: mode — "H" или "X", tplPath — задание в рабочей папке.
    // false — не запущено (уже идёт расчёт, нет ядра, нет файла); причина — в журнале.
    bool start(const QString &mode, const QString &tplPath, const QString &workDir);
    // Остановка: SIGTERM, через 2 с — SIGKILL (Windows — сразу kill: консольная
    // программа сообщение WM_CLOSE не получает). Код — 3.
    Q_INVOKABLE void cancel();

    bool running() const { return m_running; }
    double progress() const { return m_progress.percent(); }
    int step() const { return m_progress.step; }
    int stepCount() const { return m_progress.stepCount; }
    int tick() const { return m_progress.tick; }
    int tickMax() const { return m_progress.tickMax; }
    QString modelTime() const { return m_progress.modelTime; }
    qint64 elapsedMs() const;
    QString stage() const { return m_progress.stage(); }
    qint64 stageMs() const;
    QString logText() const { return m_log.join(QLatin1Char('\n')); }
    QStringList logLines() const { return m_log; }
    int exitCode() const { return m_exitCode; }
    QString mode() const { return m_mode; }

    // Добавить строку в журнал (сообщения оболочки: копирование, ошибки).
    void appendLog(const QString &line);
    void clearLog();

signals:
    void runningChanged();
    void progressChanged();   // не чаще 5 раз в секунду
    void logChanged();        // не чаще 5 раз в секунду
    void finished(int code);

private:
    void readStdout();
    void readStderr();
    void handleLine(const QString &line, bool fromStdout);
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError error);
    void finish(int code);
    void flushUi();

    QProcess *m_proc = nullptr;
    QTimer m_uiTimer;         // сглаживание обновлений экрана (≤ 5 в секунду)
    QTimer m_killTimer;
    QElapsedTimer m_clock;
    qint64 m_lastElapsed = 0;
    QString m_stage;          // стадия при последнем обновлении экрана
    qint64 m_stageStartMs = 0;
    KernelProgress m_progress;
    QStringList m_log;
    QByteArray m_outBuf, m_errBuf;
    QString m_mode;
    int m_exitCode = -1;
    bool m_running = false;
    bool m_cancelled = false;
    bool m_progressDirty = false;
    bool m_logDirty = false;
};
