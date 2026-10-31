// FieldRhiItem — сцена «Поле» на Qt RHI (QQuickRhiItem, решение M6):
// Vulkan или OpenGL ES на Android, Direct3D/Vulkan/OpenGL на настольных.
//
// Сетка узлов (u, v) — в видеопамяти один раз; значения такта — буфер, обновляемый при
// смене кадра; шкала — текстура 256×1. Рельеф, каркас, топология; перспектива 30°
// (gluPerspective(30, …) оригинала). Надписи, шкала и кнопки — Qt Quick поверх
// (FieldViewPage.qml). Вид (наклон, поворот, масштаб, сдвиг) — свойства элемента,
// поэтому переживает поворот телефона.
#pragma once

#include "touchgesture.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QQuickRhiItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace fieldview {

class FieldScene;

class FieldRhiItem : public QQuickRhiItem
{
    Q_OBJECT
    Q_MOC_INCLUDE("fieldscene.h")
    QML_ELEMENT
    Q_PROPERTY(fieldview::FieldScene *scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(double tilt READ tilt WRITE setTilt NOTIFY viewChanged)
    Q_PROPERTY(double azimuth READ azimuth WRITE setAzimuth NOTIFY viewChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(double panX READ panX WRITE setPanX NOTIFY viewChanged)
    Q_PROPERTY(double panY READ panY WRITE setPanY NOTIFY viewChanged)
    Q_PROPERTY(bool relief READ relief WRITE setRelief NOTIFY optionsChanged)
    Q_PROPERTY(bool showSurface READ showSurface WRITE setShowSurface NOTIFY optionsChanged)
    Q_PROPERTY(bool showWire READ showWire WRITE setShowWire NOTIFY optionsChanged)
    Q_PROPERTY(double reliefScale READ reliefScale WRITE setReliefScale NOTIFY optionsChanged)
    // Геометрия объёмная (блоки с высотой, как DrawTopSurface) или плоская (на плоскости
    // поля, как DrawTopSurface2) — переключатель Windows-FieldView; действует при рельефе.
    Q_PROPERTY(bool topology3D READ topology3D WRITE setTopology3D NOTIFY optionsChanged)
    Q_PROPERTY(int levels READ levels WRITE setLevels NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList portLabels READ portLabels NOTIFY portLabelsChanged)
    Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(QString backendName READ backendName NOTIFY backendNameChanged)

public:
    explicit FieldRhiItem(QQuickItem *parent = nullptr);

    FieldScene *scene() const { return m_scene; }
    void setScene(FieldScene *s);

    double tilt() const { return m_tilt; }
    void setTilt(double v);
    double azimuth() const { return m_azimuth; }
    void setAzimuth(double v);
    double zoom() const { return m_zoom; }
    void setZoom(double v);
    double panX() const { return m_panX; }
    void setPanX(double v);
    double panY() const { return m_panY; }
    void setPanY(double v);
    bool relief() const { return m_relief; }
    void setRelief(bool on);
    bool showSurface() const { return m_showSurface; }
    void setShowSurface(bool on);
    bool showWire() const { return m_showWire; }
    void setShowWire(bool on);
    double reliefScale() const { return m_reliefScale; }
    bool topology3D() const { return m_topology3D; }
    void setTopology3D(bool on);
    void setReliefScale(double v);
    int levels() const { return m_levels; }
    void setLevels(int n);
    QVariantList portLabels() const { return m_portLabels; }
    double fps() const { return m_fps; }
    QString backendName() const { return m_backendName; }

    /// Исходный вид (двойное касание): наклон и поворот по умолчанию, без сдвига и масштаба.
    Q_INVOKABLE void resetView();
    /// Поворот пальцем: dx, dy — смещение в логических точках.
    Q_INVOKABLE void rotateBy(double dx, double dy);
    /// Сдвиг двумя пальцами: смещение в логических точках.
    Q_INVOKABLE void panBy(double dx, double dy);
    /// Масштаб щипком относительно текущего.
    Q_INVOKABLE void zoomBy(double factor);
    /// Масштаб вокруг точки экрана (x, y), пиксели: точка под пальцами остаётся на месте.
    Q_INVOKABLE void zoomAt(double factor, double x, double y);

    /// Матрица вида и проекции без поправки под API (для подписей поверх сцены).
    QMatrix4x4 viewProjection(double aspect) const;

    // Для рендерера (поток отрисовки) — зовётся из FieldRenderer::synchronize.
    void reportFrame(const QString &backend);

Q_SIGNALS:
    void sceneChanged();
    void viewChanged();
    void optionsChanged();
    void portLabelsChanged();
    void fpsChanged();
    void backendNameChanged();

protected:
    void touchEvent(QTouchEvent *e) override;
    void touchUngrabEvent() override;
    QQuickRhiItemRenderer *createRenderer() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    TouchGesture m_touch;   // один палец — поворот, щипок — масштаб и сдвиг
    friend class FieldRenderer;
    void updatePortLabels();

    QPointer<FieldScene> m_scene;
    double m_tilt = -55.0;
    double m_azimuth = -20.0;
    double m_zoom = 1.0;
    double m_panX = 0.0, m_panY = 0.0;
    bool m_relief = true;
    bool m_showSurface = true;
    bool m_showWire = true;
    double m_reliefScale = 1.0;
    bool m_topology3D = true;
    int m_levels = 0;
    QVariantList m_portLabels;
    double m_fps = 0.0;
    int m_frames = 0;
    QElapsedTimer m_fpsClock;
    QString m_backendName;
};

} // namespace fieldview
