#pragma once

// Простые тела для тестов и тестовых моделей: срез каждого известен
// заранее, поэтому по ним проверяется вся цепочка «файл -> срез -> фигуры».

#include "mesh3d.h"

#include <QByteArray>
#include <QPointF>

namespace tpl::meshgen {

/// Прямоугольный параллелепипед [x0,x1] x [y0,y1] x [z0,z1].
Mesh box(const QString &name, Vec3 lo, Vec3 hi);

/// Цилиндр вдоль Z: центр основания \p c, радиус, высота, сегментов по окружности.
Mesh cylinder(const QString &name, Vec3 c, double r, double h, int seg = 64);

/// Труба (полый цилиндр) вдоль Z.
Mesh tube(const QString &name, Vec3 c, double rIn, double rOut, double h, int seg = 64);

/// Тор вокруг оси Z: большой радиус R, малый r.
Mesh torus(const QString &name, Vec3 c, double R, double r, int segR = 96, int segr = 32);

/// Призма вдоль Z по многоугольнику в плоскости XY (основание на z0, высота h).
/// Многоугольник может быть невыпуклым — грани основания ставятся веером от центра
/// масс, поэтому для невыпуклого берите звёздный относительно центра.
Mesh prism(const QString &name, const QVector<QPointF> &poly, double z0, double h);

/// Все тела одним STL: текстовый файл с `solid <имя>` на тело.
QByteArray stlAscii(const Mesh &m);
/// Двоичный STL: тела склеены в одно (формат имён не хранит).
QByteArray stlBinary(const Mesh &m);
/// OBJ: `o <имя>` и `usemtl <материал>` на тело; \p mtlName — имя файла .mtl (пусто — без него).
QByteArray obj(const Mesh &m, const QString &mtlName = QString());
/// MTL к нему: цвета тел.
QByteArray mtl(const Mesh &m);

} // namespace tpl::meshgen
