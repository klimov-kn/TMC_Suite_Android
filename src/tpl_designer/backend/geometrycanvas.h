#pragma once

// Холст геометрии встроенного TPL Designer: рисование пальцем, щипок, сетка, привязка.
//
// Правила холста — из настольного TPL Designer (src/gui/geometryview.cpp):
//  * ось Y вверх, как в задании; координаты в мм;
//  * привязка: вершины и характерные точки фигур, углы области, точки на линиях и
//    окружностях, и лишь затем шаг сетки DELTA (findSnap);
//  * металл, магнитная стенка и поглотитель рисуются КОНТУРОМ, диэлектрик и плазма —
//    ЗАЛИВКОЙ: картинка обязана совпадать с тем, что посчитает ядро;
//  * порт тянут вдоль границы: сторону задаёт направление протяжки (sideForDrag),
//    плоскость входа — поперечная координата, направление — половина коробки.
// Под палец: допуски попадания и привязки в dp крупнее, двумя пальцами — масштаб
// и сдвиг, ломаная завершается кнопкой «Готово» (Enter и двойного щелчка нет).
// Цвета приходят из QML (токены Theme) свойством colors — своих цветов здесь нет.

#include "project.h"

#include <QColor>
#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class TplDesignerModel;

class GeometryCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(TplDesignerModel *model READ model WRITE setModel NOTIFY modelChanged)
    /// select | rect | circle | line | contour | arc | port
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(bool gridVisible READ gridVisible WRITE setGridVisible NOTIFY viewChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY viewChanged)
    /// Цвета (токены Theme): bg, grid, box, boxFill, text, metal, magnetic, absorber,
    /// dielectric, plasma, plasmaB, plasmaY, active, passive, select, snap.
    Q_PROPERTY(QVariantMap colors READ colors WRITE setColors NOTIFY viewChanged)
    /// Что делает палец сейчас и что делать дальше — строка под холстом.
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)
    /// Координаты пальца (с привязкой) и к чему прилипло.
    Q_PROPERTY(QString cursorText READ cursorText NOTIFY hintChanged)
    /// Сколько точек набрано у ломаной/контура/дуги (кнопки «Готово» и «Отмена»).
    Q_PROPERTY(int chainCount READ chainCount NOTIFY hintChanged)
    Q_PROPERTY(double pixelsPerMm READ pixelsPerMm NOTIFY viewChanged)

public:
    explicit GeometryCanvas(QQuickItem *parent = nullptr);

    TplDesignerModel *model() const { return m_model; }
    void setModel(TplDesignerModel *m);
    QString tool() const { return m_tool; }
    void setTool(const QString &t);
    bool gridVisible() const { return m_grid; }
    void setGridVisible(bool on);
    bool snapEnabled() const { return m_snap; }
    void setSnapEnabled(bool on);
    QVariantMap colors() const { return m_colors; }
    void setColors(const QVariantMap &c);
    QString hint() const { return m_hint; }
    QString cursorText() const { return m_cursorText; }
    int chainCount() const { return int(m_chain.size()); }
    double pixelsPerMm() const { return m_scale; }

    Q_INVOKABLE void zoomToFit();
    Q_INVOKABLE void zoomBy(double factor, double x, double y);
    Q_INVOKABLE void panBy(double dx, double dy);
    /// Центр видимой части холста, мм — куда класть фигуру «числами».
    Q_INVOKABLE QPointF viewCenterMm() const;
    /// Размер новой фигуры «числами»: четверть видимой части, мм.
    Q_INVOKABLE double viewSizeMm() const;
    Q_INVOKABLE void finishChain();
    Q_INVOKABLE void cancelDrawing();

    void paint(QPainter *p) override;

signals:
    void modelChanged();
    void toolChanged();
    void viewChanged();
    void hintChanged();
    /// Касание по фигуре/порту, уже выделенному: показать свойства.
    void editRequested(int id);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseUngrabEvent() override;
    void wheelEvent(QWheelEvent *e) override;
    void touchEvent(QTouchEvent *e) override;
    void touchUngrabEvent() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct Snap {
        QPointF pos;
        bool toObject = false;
        QString what;
    };
    enum class Mode { None, Pan, Move, Handle, Create, Tap };

    // одна точка касания / мышь
    void pointerPress(const QPointF &pos);
    void pointerMove(const QPointF &pos);
    void pointerRelease(const QPointF &pos);
    void pointerCancel();

    QPointF toScreen(const QPointF &mm) const;
    QPointF toMm(const QPointF &screen) const;
    double gridStep() const;
    Snap findSnap(const QPointF &rawMm, bool objects) const;
    int pick(const QPointF &screen) const;
    int handleAt(const QPointF &screen) const;
    QVector<QPair<QPointF, int>> handles() const;
    void applyHandle(const QPointF &mm);
    void applyMove(const QPointF &deltaMm);
    void createFromDrag(const QPointF &endMm);
    void addChainPoint(const QPointF &mm);
    void updateHint();
    void setCursorText(const QString &t);
    QColor color(const char *key) const;
    QColor shapeColor(const tpl::Shape &s) const;
    QColor materialColor(tpl::Material m) const;
    void drawShape(QPainter *p, const tpl::Shape &s, bool selected) const;
    void drawPort(QPainter *p, const tpl::Port &port, bool selected) const;
    QString freshName(const QString &base, bool port) const;

    QPointer<TplDesignerModel> m_model;
    QString m_tool = QStringLiteral("select");
    bool m_grid = true;
    bool m_snap = true;
    QVariantMap m_colors;
    QString m_hint;
    QString m_cursorText;

    double m_scale = 2.0;    ///< пикселей (dp) на мм
    QPointF m_offset;        ///< экранное положение точки (0;0) мм
    bool m_fitted = false;
    QSizeF m_viewSize;       ///< последний ненулевой размер холста (для поворота)

    Mode m_mode = Mode::None;
    QPointF m_pressScreen;
    QPointF m_lastScreen;
    QPointF m_startMm;
    QPointF m_cursorMm;
    bool m_hasCursor = false;
    bool m_edited = false;
    int m_handle = -1;
    tpl::Shape m_origShape;
    tpl::Port m_origPort;
    bool m_origIsPort = false;
    Snap m_lastSnap;

    QVector<QPointF> m_chain;  ///< ломаная/контур/дуга: набранные точки

    // два пальца
    bool m_pinching = false;
    double m_pinchDist = 0.0;
    QPointF m_pinchCenter;
};
