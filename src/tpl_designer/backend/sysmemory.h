/// @file sysmemory.h
/// @brief Объём памяти телефона и доступная её часть (для индикатора на шаге «Проверка»).
/// @see https://www.tamic.ru/docs/code/tpl-designer.html#gui-memory
#pragma once

/// @brief Сведения о памяти устройства, запрашиваемые у системы.
namespace sysmem {

/// @brief Итог запроса памяти: удалось ли узнать, всего и доступно, байт.
struct Info {
    bool ok = false;        ///< удалось ли узнать
    double total = 0.0;     ///< байт физической памяти всего
    double available = 0.0; ///< байт можно занять, не выталкивая других

    /// @brief Возвращает занятую часть памяти.
    /// @return `total - available`, байт (0, если `ok == false`).
    double used() const { return total - available; }
};

/// @brief Читает `MemTotal` и `MemAvailable` из `/proc/meminfo`.
/// @return Сведения; `ok == false`, если файл прочитать не удалось.
Info query();

} // namespace sysmem
