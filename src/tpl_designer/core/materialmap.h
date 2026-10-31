#pragma once

// Имя материала из 3D-модели -> материал задания.
//
// В CAD материал называют как угодно: «Медь М1», «Copper», «PEC», «AISI 304»,
// «Rogers RO4003C», «Фторопласт-4». Здесь словарь, который узнаёт типовые имена, а то,
// что не узнал, честно возвращает как неизвестное — выбирает человек.

#include "project.h"

#include <QString>

namespace tpl {

struct MaterialGuess {
    bool known = false; ///< словарь узнал имя
    bool skip = false;  ///< воздух / вакуум: вставлять нечего, это фон
    Material material = Material::Metal;
    double eps = 1.0;   ///< для диэлектрика
    QString what;       ///< что узнано: «медь», «FR-4 (eps 4.4)»
};

MaterialGuess guessMaterial(const QString &name);

} // namespace tpl
