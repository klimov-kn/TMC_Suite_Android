// Цветовые шкалы поля и цвета топологии (решение M10).
//
// «Волна» — знакопеременные величины (компоненты поля в такт): −max фиолетовый,
// 0 — тёмный, +max циан. «Свечение» — неотрицательные (|E|, амплитуда, ε, N, Y, B).
// Опорные цвета — из style-guide; где они совпадают с токенами палитры (bg, surface,
// accent, accent-2, accent-3), берутся сами токены (src/ui/theme.h). Между опорными
// точками — линейная интерполяция в OKLab, как сказано в style-guide.
#pragma once

#include <QColor>
#include <QImage>
#include <QVector>

namespace fieldview {

enum class Scale { Wave = 0, Glow = 1 };

/// Опорные цвета шкалы (по возрастанию значения).
QVector<QColor> scaleStops(Scale s);
/// Цвет шкалы в точке t ∈ [0, 1].
QColor scaleColor(Scale s, double t);
/// Текстура 256×1 RGBA8 для видеокарты.
QImage scaleImage(Scale s, int width = 256);

/// Цвет и непрозрачность материала топологии по типу блока (style-guide, «Топология»).
QColor topologyColor(int blockType);

} // namespace fieldview
