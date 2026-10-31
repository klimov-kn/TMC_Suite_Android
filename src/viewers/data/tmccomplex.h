/// @file tmccomplex.h
/// @brief Комплексное число и функции модуля/фазы — перенос без изменений из библиотеки
///
/// complex Windows-версии (src/win_src/libs/complex/CMPLXALL.CPP: cabsv, cphase).
/// МАТЕМАТИКА НЕ ТРОНУТА: порядок операций и константы — как в оригинале (PI из
/// Include/Typedef.h). Фаза cphase — в диапазоне (−2π, 0], как у TMCGROUT.
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#pragma once

#include <algorithm>
#include <cmath>

namespace viewers {

struct Cplx
{
    double x = 0.0;   // действительная часть (_complex.x)
    double y = 0.0;   // мнимая часть (_complex.y)
};

namespace tmc {

inline constexpr double PI = 3.141592653589793;   // Include/Typedef.h: (_real)3.141592653589793

// CMPLXALL.CPP, cabsv: «complex absolute value».
inline double cabsv(Cplx z)
{
    double s, m, aux;
    m = std::max(std::fabs(z.x), std::fabs(z.y));   // __max( fabs(z.x), fabs(z.y) )
    if (m == 0.0) return 0.0;
    aux = z.x / m;
    s = aux * aux;
    aux = z.y / m;
    s = s + aux * aux;
    return m * std::sqrt(s);
}

// CMPLXALL.CPP, cphase.
inline double cphase(Cplx z)
{
    double fi;
    if ((z.y == 0.0) && (z.x == 0.0)) return 0.0;
    if (z.x == 0.0) {
        if (z.y > 0.0) fi = -1.5 * PI;
        else           fi = -0.5 * PI;
        return fi;
    }
    if (z.y == 0.0) {
        if (z.x > 0.0) fi = 0.0;
        else           fi = -PI;
        return fi;
    }
    fi = std::atan(z.y / z.x);
    if ((z.y > 0.0) && (z.x > 0.0)) fi = fi - 2.0 * PI;
    if ((z.y > 0.0) && (z.x < 0.0)) fi = fi - PI;
    if ((z.y < 0.0) && (z.x < 0.0)) fi = fi - PI;
    return fi;
}

} // namespace tmc
} // namespace viewers
