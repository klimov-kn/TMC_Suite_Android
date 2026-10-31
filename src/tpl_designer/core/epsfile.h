#pragma once

#include "project.h"

#include <QByteArray>
#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>

/// Файл распределения диэлектрической проницаемости `.eps` — тот самый формат, который
/// ядро пишет само (`SaveFileRect`) и читает блоком `FILE`.
///
/// 🔑 Формат двоичный и недокументированный: текстовая шапка с CRLF, шаг и начало
/// в МЕТРАХ, дальше пары «значение (double) + линейный номер узла (int)» по строкам
/// (`j = iY*nX + iX`). В значении лежит ДОБАВКА eps, умноженная на вес узла: 1 внутри,
/// 0.5 на ребре, 0.25 в углу прямоугольника. Вес не украшение — на нём держится
/// стыковка соседних блоков, и без него среда на границе получилась бы вдвое гуще.
///
/// ⚠️ Файл частотно-независим: частотный множитель ядро применяет само на каждом шаге
/// (`TmcRTHRectNode.cpp:1052`). Поэтому частоты в контексте формулы НЕТ.
///
/// Здесь нет ни одного виджета: карту на экране рисует GUI по тем же числам, что
/// уйдут в файл. Разойдясь один раз, картинка начала бы врать.
namespace tpl {

/// Сетка распределения: где лежит и какой шаг.
///
/// Координаты — ЛОКАЛЬНЫЕ, относительно анкера блока: именно так их читает ядро,
/// и именно так их пишет `SaveFileRect`. Мм.
struct EpsGrid {
    double delta = 1.0; ///< шаг, мм — обязан совпасть с DELTA задания (сверка 1e-5)
    double xMin = 0.0;
    double yMin = 0.0;
    int nX = 2;
    int nY = 2;

    double xMax() const { return xMin + (nX - 1) * delta; }
    double yMax() const { return yMin + (nY - 1) * delta; }
    double width() const { return (nX - 1) * delta; }
    double height() const { return (nY - 1) * delta; }
    int points() const { return nX * nY; }
    bool valid() const { return delta > 0.0 && nX >= 2 && nY >= 2; }
};

/// Сетка, покрывающая прямоугольник заданным шагом. Узлов не меньше двух по стороне:
/// файл из одной точки ядро прочитает, но распределением он не будет.
EpsGrid epsGridFor(double delta, double xMin, double xMax, double yMin, double yMax);

/// Байты файла `.eps`.
///
/// \p addAt — ДОБАВКА eps в точке (x, y) в локальных координатах сетки, то есть
/// `eps - 1`. Вес узла накладывается здесь, вызывающему о нём думать не нужно.
QByteArray epsFileBytes(const EpsGrid &g, const std::function<double(double, double)> &addAt);

/// То же, но центром в нуле: прямоугольник \p wMm x \p hMm с началом в `-w/2, -h/2`.
/// Так распределения задают рецепты библиотеки образцов.
QByteArray epsFileBytes(double deltaMm, double wMm, double hMm,
                        const std::function<double(double, double)> &addAt);

/// Значения добавки eps по формуле во всех узлах сетки — для карты на экране и для
/// записи файла. В формулу подставляются `x`, `y`, `r` и `f`, причём
/// ⚠️ `f` — полярный УГОЛ (радианы), а не частота: так считает ядро
/// (`TmcRTHNodeDiel.cpp:1608`).
///
/// \p error — текст ошибки разбора формулы; при ошибке возвращается пустой вектор.
QVector<double> epsValues(const EpsGrid &g, const QString &formula, QString *error = nullptr);

/// Разбор шапки готового `.eps`: сколько в нём узлов, какой шаг и где он лежит.
/// false — файл не распознан.
bool readEpsHeader(const QByteArray &data, EpsGrid *out, QString *error = nullptr);

/// Значения из готового `.eps` (уже с весами узлов, как они лежат в файле).
QVector<double> readEpsValues(const QByteArray &data, const EpsGrid &g);

} // namespace tpl
