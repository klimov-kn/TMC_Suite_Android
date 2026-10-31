#pragma once

// STEP — родной формат обмена CAD (SolidWorks, Компас-3D, FreeCAD, Fusion, Inventor).
//
// Читается библиотекой OpenCASCADE (LGPL). Если программа собрана без неё, есть
// заглушка (stepio_none.cpp): STL и OBJ работают, STEP честно отвечает «не поддержан».

#include "meshio.h"

namespace tpl {

/// Прочитать STEP: тела сборки с именами, цветами и материалами, поверхности разбиты
/// на треугольники. Координаты — в миллиметрах (OpenCASCADE пересчитывает единицы
/// файла сам), поэтому \c fileUnitMm = 1.
///
/// \p deflection — допустимое отклонение треугольников от поверхности, мм;
/// 0 — тысячная доля габарита модели.
MeshReadResult readStepFile(const QString &path, double deflection = 0.0);

/// Записать тела в STEP — нужно только для тестовых моделей.
/// Есть лишь в сборке с OpenCASCADE.
bool stepWriterAvailable();

} // namespace tpl
