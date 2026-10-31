#pragma once

#include <QVector>

#include <complex>

/// Формулы паспортов образцов: волновод, неоднородности, микрополоска, фильтры,
/// рупор, рассеяние на цилиндре. Единицы — как в редакторе: мм и ГГц.
///
/// Это ТЕОРИЯ для сравнения с тем, что посчитает ядро, а не замена ему: там, где
/// формула приближённая, в паспорте так и написано.
namespace tpl::samples::rf {

using cplx = std::complex<double>;

constexpr double kPi = 3.14159265358979323846;
/// Импеданс свободного пространства, Ом.
constexpr double kEta0 = 376.730313668;

/// Длина волны в свободном пространстве, мм.
double lambda0(double fGHz);

/// Длина волны H10 в волноводе шириной a, мм. 0 — ниже отсечки.
double lambdaGuide(double aMm, double fGHz, double eps = 1.0);

/// Тонкая симметричная индуктивная диафрагма с окном d в волноводе a (Маркувиц,
/// приближение тонкой диафрагмы): нормированная реактивная проводимость B/Y0 < 0.
double irisSusceptance(double aMm, double dMm, double fGHz);

/// Окно d, дающее нормированную реактивность X = |1/B| (обратная задача).
double irisWindowForReactance(double aMm, double x, double fGHz);

/// Тонкий центральный индуктивный штырь диаметром d (Маркувиц): нормированная
/// реактивность шунта X/Z0.
double postReactance(double aMm, double dMm, double fGHz);

/// Модуль отражения от шунтирующей реактивной проводимости jB в согласованной линии.
double shuntReflection(double b);

/// Коэффициент отражения от слоя толщиной d с проницаемостью eps (может быть
/// отрицательной — плазма) в волноводе шириной a для волны H10; a <= 0 — свободное
/// пространство, нормальное падение. Точная формула однородного слоя.
cplx slabReflection(double aMm, cplx eps, double dMm, double fGHz);

/// Прототип фильтра нижних частот: g0...g(n+1).
QVector<double> butterworth(int n);
QVector<double> chebyshev(int n, double rippleDb);

// --- микрополоска (Хаммерстад, Йенсен; квазистатика) ---
double msEpsEff(double u, double er);      ///< u = w/h
double msZ0(double u, double er);          ///< Ом
double msWidthRatio(double z0, double er); ///< синтез: w/h для заданного Z0
/// Ширина планарной волноводной модели: полоска заменена волноводом ширины W_eff с
/// магнитными стенками и заполнением eps_eff. W_eff = eta0*h/(Z0*sqrt(eps_eff)), мм.
double msPlanarWidth(double z0, double epsEff, double hMm);

// --- рассеяние ---
double besselJ(int n, double x);
double besselY(int n, double x);
/// Погонная моностатическая ЭПР идеально проводящего цилиндра радиуса a, мм.
/// tm = true — E вдоль оси цилиндра.
double cylinderEchoWidth(double aMm, double fGHz, bool tm);
/// Погонная ЭПР полосы шириной w (физическая оптика), E вдоль полосы; phi — угол
/// между направлением падения и плоскостью полосы, градусы. мм.
double stripEchoWidth(double wMm, double fGHz, double phiDeg);

/// Децибелы от модуля.
double dB(double mag);

} // namespace tpl::samples::rf
