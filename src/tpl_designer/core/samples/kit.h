#pragma once

#include "project.h"
#include "sample.h"

#include <QString>

/// Детали, из которых рецепты собирают задания: стенки волновода, входы, рамка
/// поглотителей, выбор шага сетки и времени счёта.
///
/// Всё, что здесь решается «по правилу», решается одинаково во всех образцах: шаг
/// сетки — от самой мелкой детали и длины волны, время — по пробегу волны и
/// добротности. Пользователь поправит в мастере, если захочет точнее.
namespace tpl::samples::kit {

/// Сборщик задания: раздаёт номера блоков и держит проект.
class Builder
{
public:
    explicit Builder(const QString &name);

    Project p;

    int take() { return m_next++; }

    Shape &rect(const QString &name, Material m, double x0, double x1, double y0, double y1);
    Shape &dielectric(const QString &name, double eps, double x0, double x1, double y0, double y1);
    Shape &polyline(const QString &name, Material m, const QVector<QPointF> &pts);
    Shape &polygon(const QString &name, Material m, const QVector<QPointF> &pts);
    Shape &circle(const QString &name, Material m, const QPointF &c, double r);

    /// Вход на левой/правой (апертура по Y) или нижней/верхней (по X) стороне.
    /// \p plane — координата плоскости входа, если он не на границе коробки.
    Port &port(const QString &name, PortSide side, double from, double to, bool active,
               PortMode mode = PortMode::H10);

    /// Отрезок волновода вдоль X: две металлические стенки толщиной в шаг сетки
    /// снаружи от [ylo; yhi] — так записаны авторские задания (`LR2_*.tpl`).
    void guideX(const QString &name, double x0, double x1, double ylo, double yhi);
    /// То же вдоль Y.
    void guideY(const QString &name, double y0, double y1, double xlo, double xhi);

    /// Поглотители по краям коробки (толщина — в шагах сетки). Сторона, на которой
    /// стоит вход, закрывается только выше и ниже апертуры.
    void absorberFrame(bool left, bool right, bool bottom, bool top, int cells = 5);

    /// Частоты: одна или свип.
    void freqs(double lo, double hi, int n);
    void freq(double f);

    /// Выходные файлы: всё под одним именем задания.
    ///
    /// ⚠️ Поля (FIELDS) и топология (TOPOLOGY) в образцах ВЫКЛЮЧЕНЫ: ядро переписывает
    /// всю сетку на каждом такте, и счёт растягивается в разы (антенны с полями
    /// стояли по 15 минут вместо секунд — 2026-09-23). Кому нужна картинка поля в
    /// FieldView, включает её в мастере на странице «Вывод данных».
    void outputs(bool sMatrix, bool pattern = false);

    /// Время счёта и окно снятия S-матрицы: последние 25 % счёта, но не меньше 3 нс.
    void time(double tNs);

    Build done(QVector<Row> theory, QStringList notes = {});

private:
    int m_next = 1;
};

/// Шаг сетки: не крупнее lambda/\p perLambda и мельчайшего размера/\p perFeature,
/// округлённый вниз до «круглого» (1, 2, 2.5, 5 x 10^k).
double chooseDelta(double lambdaMm, double featureMm, double perLambda = 20.0,
                   double perFeature = 4.0);

/// Шаг сетки, укладывающий ровно \p cells ячеек в \p size — чтобы стенки волновода
/// легли в узлы, а ширина по сетке совпала с номиналом.
double deltaFitting(double size, double maxDelta);

/// Округлить к ближайшему узлу сетки.
double snap(double v, double delta);

/// Время установления: пробег волны \p lengthMm с групповой скоростью у отсечки
/// \p fcGHz на частоте \p fGHz, туда-обратно, с запасом, плюс \p extraNs.
double settleTime(double lengthMm, double fGHz, double fcGHz, double extraNs = 0.0);

/// Число для паспорта: 3 значащие цифры, точка.
QString num(double v, int digits = 3);
QString ghz(double v);
QString mm(double v);
QString db(double mag);

/// Частоты свипа по полосе.
QVector<double> band(double lo, double hi, int n);

} // namespace tpl::samples::kit
