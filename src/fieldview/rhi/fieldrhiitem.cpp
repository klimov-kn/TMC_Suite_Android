/// @file fieldrhiitem.cpp
/// @brief FieldRhiItem и его рендерер на QRhi — описание в fieldrhiitem.h.
/// @see https://www.tamic.ru/docs/platforms/android.html#field

#include "fieldrhiitem.h"
#include "fieldscales.h"
#include "fieldscene.h"
#include "theme.h"

#include <QFile>
#include <QMatrix4x4>
#include <QQuickRhiItemRenderer>
#include <rhi/qrhi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace fieldview {

namespace {

constexpr double kFovY = 30.0;        // gluPerspective(30.0, …) оригинала
constexpr double kPi = 3.14159265358979323846;
constexpr double kDefaultTilt = -55.0;
constexpr double kDefaultAzimuth = -20.0;
constexpr float kTopZ3D = 0.25f;      // dZtopol: блоки топологии в 3D — от −0,25 до 0,25
constexpr float kTopZ2D = 0.02f;      // в 2D — на высоте 0,02 (DrawTopSurface2)
constexpr float kWireLift = 0.004f;   // каркас чуть выше рельефа (в 2D у Windows 0,01)

QShader loadShader(const QString &name)
{
    QFile f(QStringLiteral(":/tamic/fieldview/shaders/") + name + QStringLiteral(".qsb"));
    if (f.open(QIODevice::ReadOnly))
        return QShader::fromSerialized(f.readAll());
    return QShader();
}

/// Uniform-блок рельефа (std140, совпадает с surface.vert/.frag).
struct SurfaceUniforms
{
    float mvp[16];
    float grid[4];
    float map[4];
    float color[4];
    float misc[4];
};
static_assert(sizeof(SurfaceUniforms) == 128, "std140");

} // namespace

// --- Рендерер (поток отрисовки) ---------------------------------------------

class FieldRenderer : public QQuickRhiItemRenderer
{
public:
    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    void releaseAll();
    void buildPipelines();
    void rebuildGrid(QRhiResourceUpdateBatch *u);
    void rebuildTopology(QRhiResourceUpdateBatch *u);

    QRhi *m_rhi = nullptr;
    int m_sampleCount = 0;
    QRhiRenderPassDescriptor *m_rpDesc = nullptr;

    std::unique_ptr<QRhiBuffer> m_uvBuf, m_valBuf, m_triIdx, m_wireIdx;
    std::unique_ptr<QRhiBuffer> m_surfUbuf, m_wireUbuf, m_topoUbuf, m_topoBuf;
    std::unique_ptr<QRhiTexture> m_scaleTex;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_surfSrb, m_wireSrb, m_topoSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_surfPs, m_wirePs, m_topoPs;

    // Состояние, переданное из GUI-потока.
    RenderData m_data;
    QMatrix4x4 m_viewProj;
    bool m_relief = true, m_surface = true, m_wire = true;
    bool m_topo3D = true;      // объёмная геометрия (при рельефе)
    float m_reliefScale = 1.0f;
    int m_levels = 0;
    QColor m_bg, m_wireColor;

    // Что уже в видеопамяти.
    int m_gridX = 0, m_gridY = 0;
    quint32 m_triCount = 0, m_wireCount = 0;
    quint64 m_valVersion = ~0ull, m_scaleVersion = ~0ull, m_topoVersion = ~0ull;
    bool m_topoRelief = false;
    quint32 m_topoVertices = 0;
    quint32 m_topoCapacity = 0;
    bool m_scaleUploaded = false;
};

void FieldRenderer::releaseAll()
{
    m_surfPs.reset();
    m_wirePs.reset();
    m_topoPs.reset();
    m_surfSrb.reset();
    m_wireSrb.reset();
    m_topoSrb.reset();
    m_uvBuf.reset();
    m_valBuf.reset();
    m_triIdx.reset();
    m_wireIdx.reset();
    m_surfUbuf.reset();
    m_wireUbuf.reset();
    m_topoUbuf.reset();
    m_topoBuf.reset();
    m_scaleTex.reset();
    m_sampler.reset();
    m_gridX = m_gridY = 0;
    m_valVersion = m_scaleVersion = m_topoVersion = ~0ull;
    m_topoCapacity = 0;
    m_scaleUploaded = false;
}

void FieldRenderer::initialize(QRhiCommandBuffer *)
{
    if (m_rhi != rhi()) {
        releaseAll();
        m_rhi = rhi();
    }
    QRhiRenderTarget *rt = renderTarget();
    if (!m_surfPs || m_sampleCount != rt->sampleCount()
        || !m_rpDesc || !m_rpDesc->isCompatible(rt->renderPassDescriptor())) {
        m_sampleCount = rt->sampleCount();
        m_rpDesc = rt->renderPassDescriptor();
        buildPipelines();
    }
}

void FieldRenderer::buildPipelines()
{
    if (!m_surfUbuf) {
        m_surfUbuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(SurfaceUniforms)));
        m_surfUbuf->create();
        m_wireUbuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(SurfaceUniforms)));
        m_wireUbuf->create();
        m_topoUbuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        m_topoUbuf->create();
        m_scaleTex.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(256, 1)));
        m_scaleTex->create();
        m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        m_sampler->create();
    }
    const auto stages = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    m_surfSrb.reset(m_rhi->newShaderResourceBindings());
    m_surfSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, stages, m_surfUbuf.get()),
                            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                                      m_scaleTex.get(), m_sampler.get())});
    m_surfSrb->create();
    m_wireSrb.reset(m_rhi->newShaderResourceBindings());
    m_wireSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, stages, m_wireUbuf.get()),
                            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                                      m_scaleTex.get(), m_sampler.get())});
    m_wireSrb->create();
    m_topoSrb.reset(m_rhi->newShaderResourceBindings());
    m_topoSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                                     m_topoUbuf.get())});
    m_topoSrb->create();

    const QShader sv = loadShader(QStringLiteral("surface.vert"));
    const QShader sf = loadShader(QStringLiteral("surface.frag"));
    const QShader cv = loadShader(QStringLiteral("color.vert"));
    const QShader cf = loadShader(QStringLiteral("color.frag"));

    QRhiVertexInputLayout surfLayout;
    surfLayout.setBindings({{2 * sizeof(float)}, {sizeof(float)}});
    surfLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                              {1, 1, QRhiVertexInputAttribute::Float, 0}});

    QRhiGraphicsPipeline::TargetBlend blend;   // предумноженная альфа (шейдеры)
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    m_surfPs.reset(m_rhi->newGraphicsPipeline());
    m_surfPs->setShaderStages({{QRhiShaderStage::Vertex, sv}, {QRhiShaderStage::Fragment, sf}});
    m_surfPs->setVertexInputLayout(surfLayout);
    m_surfPs->setShaderResourceBindings(m_surfSrb.get());
    m_surfPs->setRenderPassDescriptor(m_rpDesc);
    m_surfPs->setSampleCount(m_sampleCount);
    m_surfPs->setTopology(QRhiGraphicsPipeline::Triangles);
    m_surfPs->setDepthTest(true);
    m_surfPs->setDepthWrite(true);
    m_surfPs->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    m_surfPs->setCullMode(QRhiGraphicsPipeline::None);
    m_surfPs->create();

    m_wirePs.reset(m_rhi->newGraphicsPipeline());
    m_wirePs->setShaderStages({{QRhiShaderStage::Vertex, sv}, {QRhiShaderStage::Fragment, sf}});
    m_wirePs->setVertexInputLayout(surfLayout);
    m_wirePs->setShaderResourceBindings(m_wireSrb.get());
    m_wirePs->setRenderPassDescriptor(m_rpDesc);
    m_wirePs->setSampleCount(m_sampleCount);
    m_wirePs->setTopology(QRhiGraphicsPipeline::Lines);
    m_wirePs->setDepthTest(true);
    m_wirePs->setDepthWrite(false);
    m_wirePs->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    m_wirePs->setTargetBlends({blend});
    m_wirePs->create();

    QRhiVertexInputLayout topoLayout;
    topoLayout.setBindings({{7 * sizeof(float)}});
    topoLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0},
                              {0, 1, QRhiVertexInputAttribute::Float4, 3 * sizeof(float)}});
    m_topoPs.reset(m_rhi->newGraphicsPipeline());
    m_topoPs->setShaderStages({{QRhiShaderStage::Vertex, cv}, {QRhiShaderStage::Fragment, cf}});
    m_topoPs->setVertexInputLayout(topoLayout);
    m_topoPs->setShaderResourceBindings(m_topoSrb.get());
    m_topoPs->setRenderPassDescriptor(m_rpDesc);
    m_topoPs->setSampleCount(m_sampleCount);
    m_topoPs->setTopology(QRhiGraphicsPipeline::Triangles);
    m_topoPs->setDepthTest(true);
    m_topoPs->setDepthWrite(false);
    m_topoPs->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    m_topoPs->setCullMode(QRhiGraphicsPipeline::None);
    m_topoPs->setTargetBlends({blend});
    m_topoPs->create();
}

void FieldRenderer::synchronize(QQuickRhiItem *rhiItem)
{
    auto *item = static_cast<FieldRhiItem *>(rhiItem);
    if (item->scene())
        m_data = item->scene()->renderData();
    else
        m_data = RenderData();
    const double w = std::max(1.0, item->width()), h = std::max(1.0, item->height());
    m_viewProj = item->viewProjection(w / h);
    m_relief = item->relief();
    m_topo3D = item->topology3D();
    m_surface = item->showSurface();
    m_wire = item->showWire();
    m_reliefScale = float(item->reliefScale());
    m_levels = item->levels();
    m_bg = theme::bg();
    m_wireColor = theme::line();
    if (m_rhi)
        item->reportFrame(QString::fromLatin1(m_rhi->backendName()));
}

void FieldRenderer::rebuildGrid(QRhiResourceUpdateBatch *u)
{
    const int nX = m_data.nX, nY = m_data.nY;
    m_gridX = nX;
    m_gridY = nY;
    const size_t nodes = size_t(nX) * size_t(nY);
    // Координаты узлов (u, v) — один раз на сетку.
    std::vector<float> uv(nodes * 2);
    for (int i = 0; i < nY; ++i) {
        for (int j = 0; j < nX; ++j) {
            const size_t k = (size_t(i) * size_t(nX) + size_t(j)) * 2;
            uv[k] = float(j) / float(nX - 1);
            uv[k + 1] = float(i) / float(nY - 1);
        }
    }
    m_uvBuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, quint32(uv.size() * sizeof(float))));
    m_uvBuf->create();
    u->uploadStaticBuffer(m_uvBuf.get(), uv.data());
    m_valBuf.reset(m_rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::VertexBuffer, quint32(nodes * sizeof(float))));
    m_valBuf->create();
    m_valVersion = ~0ull;

    // Треугольники: два на ячейку (четырёхугольник ii, ii+1, ii+1+nX, ii+nX оригинала).
    std::vector<quint32> tri;
    tri.reserve(size_t(nX - 1) * size_t(nY - 1) * 6);
    for (int i = 0; i < nY - 1; ++i) {
        for (int j = 0; j < nX - 1; ++j) {
            const quint32 ii = quint32(i) * quint32(nX) + quint32(j);
            tri.insert(tri.end(), {ii, ii + 1, ii + 1 + quint32(nX), ii, ii + 1 + quint32(nX), ii + quint32(nX)});
        }
    }
    m_triCount = quint32(tri.size());
    m_triIdx.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, quint32(tri.size() * sizeof(quint32))));
    m_triIdx->create();
    u->uploadStaticBuffer(m_triIdx.get(), tri.data());

    // Каркас: линии сетки, на больших сетках — через шаг (только для показа, не больше
    // ~120 линий по стороне); вдоль линии — все узлы, чтобы линия шла по рельефу.
    const int step = std::max(1, int(std::ceil(std::max(nX, nY) / 120.0)));
    std::vector<quint32> wire;
    for (int i = 0; i < nY; ++i) {
        if (i % step != 0 && i != nY - 1)
            continue;
        for (int j = 0; j < nX - 1; ++j) {
            const quint32 ii = quint32(i) * quint32(nX) + quint32(j);
            wire.insert(wire.end(), {ii, ii + 1});
        }
    }
    for (int j = 0; j < nX; ++j) {
        if (j % step != 0 && j != nX - 1)
            continue;
        for (int i = 0; i < nY - 1; ++i) {
            const quint32 ii = quint32(i) * quint32(nX) + quint32(j);
            wire.insert(wire.end(), {ii, ii + quint32(nX)});
        }
    }
    m_wireCount = quint32(wire.size());
    m_wireIdx.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, quint32(wire.size() * sizeof(quint32))));
    m_wireIdx->create();
    u->uploadStaticBuffer(m_wireIdx.get(), wire.data());
}

void FieldRenderer::rebuildTopology(QRhiResourceUpdateBatch *u)
{
    std::vector<float> v;
    const auto put = [&v](float x, float y, float z, const QColor &c) {
        v.insert(v.end(), {x, y, z, c.redF(), c.greenF(), c.blueF(), c.alphaF()});
    };
    for (const TopQuad &q : m_data.quads) {
        const QColor c = topologyColor(q.type);
        const auto face = [&](int a, float za, int b, float zb, int cc, float zc, int d, float zd) {
            put(float(q.x[a]), float(q.y[a]), za, c);
            put(float(q.x[b]), float(q.y[b]), zb, c);
            put(float(q.x[cc]), float(q.y[cc]), zc, c);
            put(float(q.x[a]), float(q.y[a]), za, c);
            put(float(q.x[cc]), float(q.y[cc]), zc, c);
            put(float(q.x[d]), float(q.y[d]), zd, c);
        };
        if (!(m_relief && m_topo3D)) {
            face(0, kTopZ2D, 1, kTopZ2D, 2, kTopZ2D, 3, kTopZ2D);   // DrawTopSurface2
        } else {
            // DrawTopSurface: верх, низ и четыре боковые грани на ±dZtopol.
            const float z = kTopZ3D;
            face(0, z, 1, z, 2, z, 3, z);
            face(0, -z, 1, -z, 2, -z, 3, -z);
            face(0, -z, 0, z, 1, z, 1, -z);
            face(1, -z, 1, z, 2, z, 2, -z);
            face(2, -z, 2, z, 3, z, 3, -z);
            face(3, -z, 3, z, 0, z, 0, -z);
        }
    }
    m_topoVertices = quint32(v.size() / 7);
    m_topoRelief = m_relief && m_topo3D;
    m_topoVersion = m_data.topologyVersion;
    if (v.empty())
        return;
    const quint32 bytes = quint32(v.size() * sizeof(float));
    if (!m_topoBuf || m_topoCapacity < bytes) {
        m_topoBuf.reset(m_rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::VertexBuffer, bytes));
        m_topoBuf->create();
        m_topoCapacity = bytes;
    }
    u->uploadStaticBuffer(m_topoBuf.get(), 0, bytes, v.data());
}

void FieldRenderer::render(QRhiCommandBuffer *cb)
{
    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
    const bool haveData = m_data.nX >= 2 && m_data.nY >= 2 && m_data.values
                          && m_data.values->size() == size_t(m_data.nX) * size_t(m_data.nY);
    if (haveData) {
        if (m_gridX != m_data.nX || m_gridY != m_data.nY)
            rebuildGrid(u);
        if (m_valVersion != m_data.valuesVersion) {
            // Значения такта — каждый новый кадр анимации.
            u->uploadStaticBuffer(m_valBuf.get(), 0, quint32(m_data.values->size() * sizeof(float)),
                                  m_data.values->data());
            m_valVersion = m_data.valuesVersion;
        }
        if (m_scaleVersion != m_data.scaleVersion || !m_scaleUploaded) {
            u->uploadTexture(m_scaleTex.get(), scaleImage(m_data.scale, 256));
            m_scaleVersion = m_data.scaleVersion;
            m_scaleUploaded = true;
        }
        if (m_topoVersion != m_data.topologyVersion || m_topoRelief != (m_relief && m_topo3D))
            rebuildTopology(u);
    }

    const QMatrix4x4 mvp = m_rhi->clipSpaceCorrMatrix() * m_viewProj;
    SurfaceUniforms su;
    std::memcpy(su.mvp, mvp.constData(), 64);
    const float hs = m_relief ? m_reliefScale : 0.0f;
    su.grid[0] = float(m_data.sizeX);
    su.grid[1] = float(m_data.sizeY);
    su.grid[2] = hs;
    su.grid[3] = 0.0f;
    su.map[0] = float(m_data.k);
    su.map[1] = float(m_data.off);
    su.map[2] = float(m_data.colorMode);
    su.map[3] = float(m_levels);
    su.color[0] = su.color[1] = su.color[2] = su.color[3] = 1.0f;
    su.misc[0] = 1.0f;
    su.misc[1] = m_relief ? 0.35f : 0.0f;
    su.misc[2] = su.misc[3] = 0.0f;
    SurfaceUniforms wu = su;
    wu.grid[3] = m_relief ? kWireLift : 0.01f;
    // Каркас поверх рельефа — цвет line (style-guide); без рельефа — по шкале, как
    // DrawFieldLine оригинала без заливки.
    if (m_surface) {
        wu.misc[0] = 0.0f;
        wu.color[0] = m_wireColor.redF();
        wu.color[1] = m_wireColor.greenF();
        wu.color[2] = m_wireColor.blueF();
        wu.color[3] = 0.9f;
    }
    wu.misc[1] = 0.0f;
    u->updateDynamicBuffer(m_surfUbuf.get(), 0, sizeof(su), &su);
    u->updateDynamicBuffer(m_wireUbuf.get(), 0, sizeof(wu), &wu);
    u->updateDynamicBuffer(m_topoUbuf.get(), 0, 64, mvp.constData());

    cb->beginPass(renderTarget(), m_bg, {1.0f, 0}, u);
    if (haveData) {
        const QSize sz = renderTarget()->pixelSize();
        const QRhiViewport vp(0, 0, float(sz.width()), float(sz.height()));
        const QRhiCommandBuffer::VertexInput vin[] = {{m_uvBuf.get(), 0}, {m_valBuf.get(), 0}};
        if (m_surface) {
            cb->setGraphicsPipeline(m_surfPs.get());
            cb->setViewport(vp);
            cb->setShaderResources();
            cb->setVertexInput(0, 2, vin, m_triIdx.get(), 0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(m_triCount);
        }
        if (m_wire) {
            cb->setGraphicsPipeline(m_wirePs.get());
            cb->setViewport(vp);
            cb->setShaderResources();
            cb->setVertexInput(0, 2, vin, m_wireIdx.get(), 0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(m_wireCount);
        }
        if (m_topoVertices > 0 && m_topoBuf) {
            const QRhiCommandBuffer::VertexInput tin(m_topoBuf.get(), 0);
            cb->setGraphicsPipeline(m_topoPs.get());
            cb->setViewport(vp);
            cb->setShaderResources();
            cb->setVertexInput(0, 1, &tin);
            cb->draw(m_topoVertices);
        }
    }
    cb->endPass();
}

// --- Элемент (GUI-поток) -----------------------------------------------------

FieldRhiItem::FieldRhiItem(QQuickItem *parent) : QQuickRhiItem(parent)
{
    // Касания разбирает TouchGesture: поворот не может начаться во время щипка.
    setAcceptTouchEvents(true);
    m_touch.onDrag = [this](const QPointF &d) { rotateBy(d.x(), d.y()); };
    m_touch.onPinch = [this](double f, const QPointF &c, const QPointF &d) {
        panBy(d.x(), d.y());
        zoomAt(f, c.x(), c.y());
    };
    m_touch.onDoubleTap = [this](const QPointF &) { resetView(); };
    setSampleCount(4);   // сглаживание линий каркаса и краёв рельефа
    m_fpsClock.start();
}

QQuickRhiItemRenderer *FieldRhiItem::createRenderer()
{
    return new FieldRenderer;
}

void FieldRhiItem::setScene(FieldScene *s)
{
    if (s == m_scene)
        return;
    if (m_scene)
        disconnect(m_scene, nullptr, this, nullptr);
    m_scene = s;
    if (m_scene) {
        connect(m_scene, &FieldScene::renderDataChanged, this, [this]() { update(); });
        connect(m_scene, &FieldScene::topologyChanged, this, [this]() {
            updatePortLabels();
            update();
        });
    }
    Q_EMIT sceneChanged();
    updatePortLabels();
    update();
}

#define FV_SETTER(Name, member, sig, extra) \
    void FieldRhiItem::Name(decltype(member) v) \
    { \
        if (member == v) \
            return; \
        member = v; \
        extra; \
        Q_EMIT sig(); \
        updatePortLabels(); \
        update(); \
    }

FV_SETTER(setTilt, m_tilt, viewChanged, m_tilt = std::clamp(m_tilt, -180.0, 180.0))
FV_SETTER(setAzimuth, m_azimuth, viewChanged, m_azimuth = std::remainder(m_azimuth, 360.0))
FV_SETTER(setZoom, m_zoom, viewChanged, m_zoom = std::clamp(m_zoom, 0.2, 40.0))
FV_SETTER(setPanX, m_panX, viewChanged, (void)0)
FV_SETTER(setPanY, m_panY, viewChanged, (void)0)
FV_SETTER(setRelief, m_relief, optionsChanged, (void)0)
FV_SETTER(setShowSurface, m_showSurface, optionsChanged, (void)0)
FV_SETTER(setShowWire, m_showWire, optionsChanged, (void)0)
FV_SETTER(setReliefScale, m_reliefScale, optionsChanged, m_reliefScale = std::clamp(m_reliefScale, 0.05, 10.0))
FV_SETTER(setLevels, m_levels, optionsChanged, (void)0)
FV_SETTER(setTopology3D, m_topology3D, optionsChanged, (void)0)

#undef FV_SETTER

void FieldRhiItem::resetView()
{
    m_tilt = m_relief ? kDefaultTilt : 0.0;
    m_azimuth = m_relief ? kDefaultAzimuth : 0.0;
    m_zoom = 1.0;
    m_panX = m_panY = 0.0;
    Q_EMIT viewChanged();
    updatePortLabels();
    update();
}

void FieldRhiItem::rotateBy(double dx, double dy)
{
    m_azimuth = std::remainder(m_azimuth + dx * 0.4, 360.0);
    m_tilt = std::clamp(m_tilt + dy * 0.4, -180.0, 180.0);
    Q_EMIT viewChanged();
    updatePortLabels();
    update();
}

void FieldRhiItem::panBy(double dx, double dy)
{
    if (width() <= 0 || height() <= 0)
        return;
    m_panX += 2.0 * dx / width();
    m_panY -= 2.0 * dy / height();
    Q_EMIT viewChanged();
    updatePortLabels();
    update();
}

void FieldRhiItem::zoomBy(double factor)
{
    setZoom(m_zoom * factor);
}

void FieldRhiItem::zoomAt(double factor, double x, double y)
{
    if (width() <= 0 || height() <= 0)
        return;
    const double before = m_zoom;
    setZoom(m_zoom * factor);
    const double f = before > 0 ? m_zoom / before : 1.0;   // setZoom мог ограничить масштаб
    const double nx = 2.0 * x / width() - 1.0;
    const double ny = 1.0 - 2.0 * y / height();
    m_panX = nx - (nx - m_panX) * f;
    m_panY = ny - (ny - m_panY) * f;
    Q_EMIT viewChanged();
    updatePortLabels();
    update();
}

void FieldRhiItem::touchEvent(QTouchEvent *e)
{
    m_touch.process(e);
}

void FieldRhiItem::touchUngrabEvent()
{
    m_touch.reset();
}

QMatrix4x4 FieldRhiItem::viewProjection(double aspect) const
{
    double sx = 2.5, sy = 2.5;
    if (m_scene) {
        sx = m_scene->renderData().sizeX;
        sy = m_scene->renderData().sizeY;
    }
    // Расстояние камеры подобрано так, чтобы поле целиком помещалось при виде по
    // умолчанию в любом положении экрана (у Windows — постоянные −4,5 по Z).
    const double t = std::tan(kFovY / 2 * kPi / 180.0);
    const double a = kDefaultAzimuth * kPi / 180.0, tl = kDefaultTilt * kPi / 180.0;
    const double hx = std::fabs(std::cos(a)) * sx / 2 + std::fabs(std::sin(a)) * sy / 2;
    const double hy0 = std::fabs(std::sin(a)) * sx / 2 + std::fabs(std::cos(a)) * sy / 2;
    const double hz = m_relief ? m_reliefScale : 0.0;
    const double hy = std::max(hy0, hy0 * std::fabs(std::cos(tl)) + hz * std::fabs(std::sin(tl)));
    const double dist = std::max(hx / (t * aspect), hy / t) * 1.18 + 0.5;
    const double r = 0.5 * std::sqrt(sx * sx + sy * sy + 4 * hz * hz) * m_zoom;

    QMatrix4x4 proj;
    proj.perspective(float(kFovY), float(aspect), float(std::max(0.01, (dist - r) * 0.5)),
                     float(dist + r + 1.0));
    QMatrix4x4 view;
    view.translate(float(m_panX * dist * t * aspect), float(m_panY * dist * t), float(-dist));
    view.rotate(float(m_tilt), 1, 0, 0);     // glRotatef(wAngleX, 1, 0, 0)
    view.rotate(float(m_azimuth), 0, 0, 1);  // glRotatef(wAngleZ, 0, 0, 1)
    view.scale(float(m_zoom));
    return proj * view;
}

void FieldRhiItem::updatePortLabels()
{
    QVariantList out;
    if (m_scene && width() > 0 && height() > 0) {
        const QMatrix4x4 vp = viewProjection(width() / height());
        const float z = m_relief && m_topology3D ? kTopZ3D : kTopZ2D;
        for (const TopPort &p : m_scene->scenePorts()) {
            const QVector4D c = vp * QVector4D(float(p.x), float(p.y), z, 1.0f);
            if (c.w() <= 0.0f)
                continue;
            const double nx = c.x() / c.w(), ny = c.y() / c.w();
            if (nx < -1.2 || nx > 1.2 || ny < -1.2 || ny > 1.2)
                continue;
            QVariantMap m;
            m.insert(QStringLiteral("text"), QStringLiteral("Inp %1").arg(p.number));   // как DrawTopInputNum
            m.insert(QStringLiteral("x"), (nx + 1.0) / 2.0 * width());
            m.insert(QStringLiteral("y"), (1.0 - ny) / 2.0 * height());
            out << m;
        }
    }
    m_portLabels = out;
    Q_EMIT portLabelsChanged();
}

void FieldRhiItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickRhiItem::geometryChange(newGeometry, oldGeometry);
    updatePortLabels();
}

void FieldRhiItem::reportFrame(const QString &backend)
{
    // Зовётся из synchronize (GUI-поток ждёт) — сигналы отправляются очередью.
    ++m_frames;
    const qint64 ms = m_fpsClock.elapsed();
    const bool newBackend = backend != m_backendName;
    if (newBackend)
        m_backendName = backend;
    if (ms >= 1000 || newBackend) {
        if (ms >= 1000) {
            m_fps = m_frames * 1000.0 / double(ms);
            m_frames = 0;
            m_fpsClock.restart();
        }
        QMetaObject::invokeMethod(this, [this, newBackend]() {
            Q_EMIT fpsChanged();
            if (newBackend)
                Q_EMIT backendNameChanged();
        }, Qt::QueuedConnection);
    }
}

} // namespace fieldview
