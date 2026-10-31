#pragma once

#include "project.h"

#include <QString>
#include <QStringList>

namespace tpl {

/// Метка служебной строки углового свипа в шапке задания. По ней чтение
/// восстанавливает свип (см. `sweepHeader` в emitter.cpp).
inline constexpr const char *kSweepMarker = "@TPLD-SWEEP";

/// Макро-версия задания: файлы обязаны лежать рядом.
struct MacroFiles {
    QString tpl;       ///< содержимое <name>.tpl
    QString stepH;     ///< содержимое <name>_step.h
    QString stepHName; ///< имя .h для строки #include
};

struct Diagnostics {
    /// Файл писать нельзя: ядро не примет или молча посчитает не то.
    QStringList errors;

    /// Файл корректен, но что-то стоит знать. Решает пользователь — не запрещаем.
    QStringList warnings;

    bool ok() const { return errors.isEmpty(); }
};

Diagnostics validate(const Project &p);

/// Одна частота — один #STEP.
QString emitMono(const Project &p);

/// Диапазон частот — макрос: шаг описан ОДИН раз, число шагов = числу строк вызова.
///
/// Если поля просят только на первом шаге, первый шаг пишется явно, а остальные —
/// вызовами: тело макроса одно на все частоты, «первого» внутри него не существует.
MacroFiles emitMacro(const Project &p);

/// Больше одной частоты — макрос. Две частоты уже повод не плодить копипасту.
bool needsMacro(const Project &p);

/// Одно задание конвейера: своя часть сетки частот и свои выходные файлы.
struct TaskChunk {
    QString name;          ///< имя задания без расширения: `wg_001`
    QString tpl;           ///< содержимое .tpl
    QString stepH;         ///< содержимое _step.h; пусто — моно-версия
    QString stepHName;     ///< имя .h для строки #include
    QVector<double> freqs; ///< частоты именно этого задания
    /// Файлы `.s` этого задания, без расширения. Их удаляют ПЕРЕД прогоном: ядро
    /// дописывает матрицу, а не перезаписывает.
    QStringList sMatrixFiles;
};

/// Разложить сетку частот по файлам задания.
///
/// Без конвейера возвращает ОДИН элемент — то же самое задание. Поэтому вызывающему
/// не нужно ветвление «делим или нет»: путь всегда один.
///
/// Каждому заданию дают СВОИ имена выходных файлов: иначе `.s` (он дописывается, а не
/// перезаписывается) перемешал бы точки разных заданий, а при запуске пачкой — ещё и
/// испортился бы от одновременной записи. Файл падающей волны, наоборот, ОБЩИЙ: он
/// один и тот же для всех частей, и плодить сотню копий незачем.
QVector<TaskChunk> emitChunks(const Project &p);

/// Выражение для параметра Amp блока INPUT_*.
///
/// Ядро считает a = Amp(t,f)*sin(2*pi*FREQ*t + Faza(t,f)) и режет резким окном.
/// Amp — не число, а выражение от t (нс) и f (ГГц): огибающая живёт здесь.
/// Интерпретатор понимает abs/exp/log/sin/cos/tan/sqrt/sinh/cosh/tanh/asin/acos/atan/log10
/// (строчными, со скобкой) и степень '^'.
QString ampExpression(const Signal &s);

/// Окно возбуждения порта, нс. У пассивного начинается заведомо позже конца счёта.
TimeWindow portWindow(const Port &p, double timeMax);

/// 60 -> "60.0", 0.071 -> "0.071", 3.1e17 -> "310000000000000000.0".
/// Экспоненты не бывает НИКОГДА: вычислитель выражений ядра её не понимает.
QString formatNumber(double v);
/// "3.1e+17" -> "310000000000000000", "1.5e-05" -> "0.000015"; без экспоненты — как есть.
QString plainDecimal(const QString &number);
/// Все числа с экспонентой в выражении — полной десятичной записью.
QString plainLiterals(const QString &expr);
/// Без хвостовой ".0": 115 -> "115".
QString formatCompact(double v);
/// FREQ — всегда один знак после точки.
QString formatFreq(double ghz);

} // namespace tpl
