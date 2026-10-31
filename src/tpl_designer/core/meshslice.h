#pragma once

// Срез 3D-модели плоскостью: из треугольной сетки — контуры на плоскости холста.

#include "mesh3d.h"

#include <QPointF>
#include <QVector>

namespace tpl {

/// Плоскость среза. Название — две оси, которые ложатся на холст.
enum class SlicePlane { XY, YZ, XZ };

QString slicePlaneName(SlicePlane p);

/// Какая ось модели — нормаль плоскости, какие две идут в X и Y холста
/// (0 — x, 1 — y, 2 — z). XY: (x, y), нормаль z; YZ: (y, z), нормаль x; XZ: (x, z), нормаль y.
struct PlaneAxes {
    int u = 0, v = 1, normal = 2;
};
PlaneAxes planeAxes(SlicePlane p);
double axisValue(const Vec3 &p, int axis);
/// Название нормали: «Z» для XY и т. д. — подпись ползунка.
QString normalAxisName(SlicePlane p);

/// Один контур среза.
struct SliceLoop {
    QVector<QPointF> pts; ///< в координатах плоскости (u, v), мм
    bool closed = false;  ///< замкнулся ли
    /// Вложенность среди замкнутых контуров ТОГО ЖЕ тела: 0 — внешний, 1 — дырка в нём,
    /// 2 — остров в дырке… Нужна для заливки диэлектрика (чётно-нечётное правило).
    int depth = 0;
};

struct SliceBody {
    int body = 0; ///< номер тела в Mesh::bodies
    QVector<SliceLoop> loops;
};

struct SliceResult {
    QVector<SliceBody> bodies; ///< только тела, которые плоскость задела
    int openLoops = 0;         ///< сколько контуров не замкнулось (сетка «не герметична»)
};

/// Разрезать сетку плоскостью «координата нормали = offset».
///
/// Сетка должна быть склеена (`Mesh::weld`): отрезки сшиваются по общим рёбрам.
/// Вершина ровно на плоскости считается лежащей «выше» неё — так у отрезков не
/// бывает вырожденных случаев, а грань, лежащая в самой плоскости, не даёт ничего
/// (её контур дадут соседние боковые грани).
SliceResult sliceMesh(const Mesh &mesh, SlicePlane plane, double offset);

/// Упростить ломаную: убрать точки, отклоняющиеся от прямой меньше \p tol (Дуглас — Пейкер).
QVector<QPointF> simplifyPolyline(const QVector<QPointF> &pts, double tol, bool closed);

/// Упростить все контуры среза (замкнутым — не меньше трёх точек).
void simplifySlice(SliceResult *r, double tol);

/// Точка внутри многоугольника (чётно-нечётное правило).
bool pointInPolygon(const QPointF &p, const QVector<QPointF> &poly);

} // namespace tpl
