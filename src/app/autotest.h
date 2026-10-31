// Autotest — режим самопроверки: считает встроенные примеры тем же путём, что и
// человек (CalcSession: копирование в рабочую папку, KernelRunner, копирование
// результатов обратно), и пишет итог в журнал (logcat, тег TAMIC) и в файл.
//
// Запуск:
//   Android:  am start -n ru.tamic.app/org.qtproject.qt.android.bindings.QtActivity
//             -e autotest voln_h|voln_x|horn_h|horn_x|plasma_x|all
//   desktop:  tamic --autotest all
// Папка задач самопроверки: <внешняя папка>/autotest/<случай>/ (Android —
// /sdcard/Android/data/ru.tamic.app/files/autotest, её забирает «adb pull»);
// отчёт — <данные>/autotest/report.txt и копия рядом с результатами.
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>

class Autotest : public QObject
{
    Q_OBJECT
public:
    struct Case { QString id; QString example; QString tpl; QString mode; };
    static QList<Case> allCases();
    // «all» или имена через запятую. Пустой список — неизвестное имя.
    static QList<Case> parse(const QString &spec);

    Autotest(const QList<Case> &cases, bool quitWhenDone, QObject *parent = nullptr);
    void start();

signals:
    void done(bool ok);

private:
    void next();
    void onFinished(int code);
    void finishAll();
    void line(const QString &text);

    QList<Case> m_cases;
    int m_index = -1;
    int m_passed = 0;
    bool m_quit = false;
    QStringList m_report;
    QString m_baseDir;
    QElapsedTimer m_total;
};
