#pragma once

// Треугольная сетка 3D-модели — то, во что превращается любой принесённый файл
// (STL, OBJ, STEP), прежде чем его разрежут плоскостью.
//
// Только Qt::Core: модель читается и режется без окон, и это проверяют тесты.

#include <QString>
#include <QVector>

#include <array>
#include <cstdint>

namespace tpl {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

/// Габарит модели.
struct Bounds3 {
    Vec3 lo, hi;
    bool valid = false;
    Vec3 center() const { return {(lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2}; }
    double size() const; ///< диагональ
};

/// Тело модели: кусок сетки с одним именем и одним материалом.
///
/// Из STL это `solid <имя>` или сам файл, из OBJ — объект/группа с `usemtl`, из STEP —
/// тело сборки. Срез режет тела ПОРОЗНЬ: у каждого свой материал, и сливать их контуры
/// нельзя — граница меди и тефлона есть граница двух фигур.
struct MeshBody {
    QString name;     ///< имя тела, как в файле
    QString material; ///< имя материала, как в файле (пусто — не задан)
    bool hasColor = false;
    std::uint32_t rgb = 0xA0A8B8; ///< цвет из файла — только для показа
    int firstTri = 0;
    int triCount = 0;
};

/// Сетка: общие вершины, треугольники по три номера, тела — непрерывные куски треугольников.
struct Mesh {
    QVector<Vec3> vertices;
    QVector<std::array<int, 3>> tris;
    QVector<MeshBody> bodies;

    bool isEmpty() const { return tris.isEmpty(); }
    Bounds3 bounds() const;

    /// Склеить совпадающие вершины.
    ///
    /// 🔑 Без этого срез не собирается в контуры. STL хранит каждый треугольник со
    /// СВОИМИ тремя вершинами, и у соседних треугольников общая вершина записана
    /// дважды. Склеенная сетка даёт рёбрам общие номера, а по номеру ребра отрезки
    /// среза сшиваются ТОЧНО, без допусков по расстоянию.
    /// \p tol — расстояние, ближе которого вершины считаются одной; 0 — доля габарита.
    void weld(double tol = 0.0);

    /// Масштаб: единицы файла -> мм.
    void scale(double k);

    /// Добавить тело из другой сетки (несколько файлов STL — несколько тел).
    void append(const Mesh &other);
};

} // namespace tpl
