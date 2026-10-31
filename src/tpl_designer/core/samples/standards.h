#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/// Справочные таблицы библиотеки образцов: сечения волноводов и СВЧ-подложки.
namespace tpl::samples {

/// Прямоугольный волновод стандартного сечения.
///
/// ⚠️ Ядро двумерное и считает H-плоскость: в расчёт входит только широкая стенка `a`.
/// Узкая `b` — справка из стандарта, на задание она не влияет.
struct WaveguideStd {
    QString name;     ///< «WR-90 / R100» или «ГОСТ 23x10»
    QString family;   ///< «EIA / IEC» или «ГОСТ 20900-75»
    double a = 0.0;   ///< широкая стенка, мм
    double b = 0.0;   ///< узкая стенка, мм (справка)
    double fLo = 0.0; ///< рабочий диапазон, ГГц
    double fHi = 0.0;
    bool bandFromStandard = false; ///< диапазон из стандарта, а не расчётный 1.25..1.9 fc

    double fc10() const;          ///< отсечка H10, ГГц
    QString label() const;        ///< строка выпадающего списка
};

const QVector<WaveguideStd> &waveguides();
QStringList waveguideChoices();
/// Номер WR-90 в таблице — умолчание для большинства образцов (X-диапазон, 8.2-12.4 ГГц).
int defaultWaveguide();
/// Номер ближайшего стандарта, в полосе которого лежит частота.
int waveguideForFreq(double fGHz);

/// Подложка для микрополосковых образцов.
struct Substrate {
    QString name;
    double eps = 1.0;  ///< относительная проницаемость
    double h = 1.0;    ///< стандартная толщина, мм
    QString note;      ///< откуда число
};

const QVector<Substrate> &substrates();
QStringList substrateChoices();

} // namespace tpl::samples
