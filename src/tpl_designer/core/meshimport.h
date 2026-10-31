#pragma once

// Срез 3D-модели -> фигуры задания.
//
// Вставляются ОБЫЧНЫЕ фигуры — те же, что рисует человек: эмиттер, чтение `.tpl`,
// поворот и проверка про импорт не знают ничего. Связи с моделью после вставки нет.

#include "meshslice.h"
#include "project.h"

#include <QStringList>

namespace tpl {

/// Что делать с телом модели.
struct BodyChoice {
    bool include = true;
    Material material = Material::Metal;
    double eps = 1.0; ///< для диэлектрика — сама eps, как везде в программе
};

/// Как срез ложится на холст: поворот, зеркало, сдвиг.
struct ImportPlacement {
    int rot90 = 0;       ///< поворот на 90 градусов * rot90 против часовой
    bool mirror = false; ///< отразить по горизонтали (до поворота)
    QPointF shift;       ///< сдвиг после поворота, мм
};

QPointF placePoint(const QPointF &uv, const ImportPlacement &pl);

/// Сдвиг, который ставит центр габарита среза в центр счётной области.
QPointF centeringShift(const SliceResult &r, ImportPlacement pl, const Box &box);

struct ImportShapes {
    QVector<Shape> shapes;
    QStringList notes; ///< что сделано не один в один: незамкнутые контуры, разбиение
    int dielectricPieces = 0;
};

/// Собрать фигуры. \p choices — по телу на каждый элемент `mesh.bodies`. Номера
/// фигур — подряд с \p firstId. Срез уже упрощён (`simplifySlice`).
///
/// * Металл, магнитная стенка, поглотитель — контуром: замкнутый срез — замкнутый
///   контур, незамкнутый — ломаная. Ядро рисует их по периметру, выпуклость не нужна.
/// * Диэлектрик — заливкой, а заливку ядро умеет только выпуклую: область режется на
///   выпуклые куски (`convexPieces`). Незамкнутый контур залить нельзя — о нём
///   говорится в notes, и он не вставляется.
ImportShapes buildImportShapes(const Mesh &mesh, const SliceResult &slice,
                               const QVector<BodyChoice> &choices, const ImportPlacement &pl,
                               int firstId);

/// Строка для описания задания: откуда взята геометрия.
QString sliceProvenance(const QString &fileName, SlicePlane plane, double offset);

} // namespace tpl
