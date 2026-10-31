#pragma once

#include <cmath>

/// Формулы среды и волновода — одни и те же для валидатора и для калькулятора в GUI.
///
/// Живут в ядре, а не в диалоге, потому что валидатор обязан предупреждать теми же
/// числами, которые пользователь видит в калькуляторе. Разъехавшиеся формулы — это
/// «программа сказала одно, а посчиталось другое», то есть ровно та ошибка, ради
/// которой проект и затевался.
///
/// Заголовок сделан header-only намеренно: `src/core/CMakeLists.txt` перечисляет
/// исходники поимённо, и новый .cpp потребовал бы его править.
namespace tpl {

/// Скорость света в мм*ГГц: даёт частоту в ГГц, когда размер задан в мм.
/// ⚠️ Легко ошибиться на 1000x, взяв км/с.
constexpr double kLightSpeed = 299.792458;

/// f_p [Гц] = 8.9788 * sqrt(N), N в м^-3 (СИ).
/// Проверка: N = 3.1e17 -> 5.0 ГГц — ровно та плазма, на которой считаются ЛР2.
constexpr double kPlasmaK = 8.9788;

/// f_ce [Гц] = 2.79925e10 * B, B в Тесла. Проверка: B = 0.071 Тл -> 1.99 ГГц.
constexpr double kCyclotronK = 2.79925e10;

/// Плазменная частота, ГГц, по концентрации в м^-3.
inline double plasmaFreqGHz(double n)
{
    return n > 0.0 ? kPlasmaK * std::sqrt(n) / 1e9 : 0.0;
}

/// Обратный ход: какая концентрация даёт заданную плазменную частоту.
inline double concentrationForFreqGHz(double fGHz)
{
    const double fp = fGHz * 1e9;
    return fp > 0.0 ? std::pow(fp / kPlasmaK, 2.0) : 0.0;
}

/// Циклотронная частота, ГГц, по индукции в Тесла.
inline double cyclotronFreqGHz(double b)
{
    return kCyclotronK * std::fabs(b) / 1e9;
}

/// Обратный ход: какое поле даёт заданную циклотронную частоту.
inline double fieldForFreqGHz(double fGHz)
{
    return fGHz * 1e9 / kCyclotronK;
}

/// Характерные частоты замагниченной плазмы, ГГц.
///
/// X-мода (E перпендикулярно B) НЕ распространяется в двух полосах:
///   f < f_L                и                f_UH < f < f_R.
/// Без подмагничивания (B = 0) все три частоты вырождаются в f_p, и остаётся
/// единственная отсечка f < f_p — привычный случай изотропной плазмы.
struct PlasmaBands {
    double fp = 0.0;  ///< плазменная
    double fce = 0.0; ///< циклотронная
    double fL = 0.0;  ///< нижняя отсечка (левая)
    double fR = 0.0;  ///< верхняя отсечка (правая)
    double fUH = 0.0; ///< верхний гибридный резонанс
};

inline PlasmaBands plasmaBands(double n, double b)
{
    PlasmaBands r;
    r.fp = plasmaFreqGHz(n);
    r.fce = cyclotronFreqGHz(b);
    const double root = std::sqrt(r.fce * r.fce / 4.0 + r.fp * r.fp);
    r.fL = root - r.fce / 2.0;
    r.fR = root + r.fce / 2.0;
    r.fUH = std::sqrt(r.fp * r.fp + r.fce * r.fce);
    return r;
}

/// Непрозрачна ли плазма для X-моды на этой частоте.
inline bool isPlasmaOpaque(const PlasmaBands &pb, double fGHz)
{
    if (pb.fp <= 0.0)
        return false; // плазмы нет — прозрачно всегда
    if (fGHz < pb.fL)
        return true;
    return fGHz > pb.fUH && fGHz < pb.fR;
}

/// Отсечка волны H10 в волноводе шириной a (мм), ГГц. Ниже неё волна не пойдёт.
inline double cutoffH10(double aMm, double eps = 1.0)
{
    const double nRefr = std::sqrt(eps > 0.0 ? eps : 1e-6);
    return kLightSpeed / (2.0 * aMm * nRefr);
}

/// Отсечка H20: выше неё вход многомодовый и S-матрица смысла не имеет.
inline double cutoffH20(double aMm, double eps = 1.0)
{
    return 2.0 * cutoffH10(aMm, eps);
}

} // namespace tpl
