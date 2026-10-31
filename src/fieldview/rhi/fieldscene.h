// FieldScene — данные экрана «Поле» для QML: какие файлы есть в папке результатов,
// серия тактов, текущий такт, слой (поле / ε / N / Y / B), шкала и её пределы,
// топология. Рисует FieldRhiItem (fieldrhiitem.h), он берёт отсюда renderData().
//
// Обработка значений — как в CFldViewView::PrepareData / PrepareTpl / SetColor
// Windows-версии (src/win_src/fieldview/FldViewView.cpp):
//   высота h = 2·((v − zMin)/(zMax − zMin) − 0,5) ∈ [−1, 1] (ZtoGLWinCoord);
//   цвет: обычный режим — (h + 1)/2, режим «Модуль» — |h| (SetColor);
//   слои ε/N/Y/B — от минимума до максимума сетки (Z_EpstoGLWinCoord, SetColor_Eps).
// Отличия (решение M27, ISSUES «О4»): пределы считаются заново для каждого кадра или
// по всей серии, без «залипания» прошлых значений (С-57); шкала «Волна» симметрична
// относительно нуля, чтобы ноль был тёмным (style-guide); min == max не делит на ноль (С-56).
#pragma once

#include "fieldfile.h"
#include "fieldscales.h"
#include "topology.h"

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <list>
#include <memory>
#include <unordered_map>

class QThread;

namespace fieldview {

/// Снимок того, что рисовать (копируется в поток отрисовки в synchronize()).
struct RenderData
{
    int nX = 0, nY = 0;
    double sizeX = 2.5, sizeY = 2.5;   // размеры поля в координатах сцены (XtoGLWinCoord)
    std::shared_ptr<const std::vector<float>> values;   // значения узлов слоя
    quint64 valuesVersion = 0;
    double k = 1.0, off = 0.0;   // h = v·k + off
    int colorMode = 0;           // 0 — (h+1)/2, 1 — |h|
    Scale scale = Scale::Wave;
    quint64 scaleVersion = 0;
    std::vector<TopQuad> quads;  // в координатах сцены (x, y)
    quint64 topologyVersion = 0;
};

class FieldScene : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    Q_PROPERTY(QStringList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(int sourceIndex READ sourceIndex WRITE setSourceIndex NOTIFY sourceIndexChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(QString error READ error NOTIFY loadedChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY loadedChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY loadedChanged)
    Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
    Q_PROPERTY(int nT READ nT NOTIFY frameChanged)
    Q_PROPERTY(QString timeText READ timeText NOTIFY frameChanged)
    Q_PROPERTY(int nX READ nX NOTIFY loadedChanged)
    Q_PROPERTY(int nY READ nY NOTIFY loadedChanged)
    Q_PROPERTY(QString gridText READ gridText NOTIFY loadedChanged)
    Q_PROPERTY(bool hasTopology READ hasTopology NOTIFY loadedChanged)
    Q_PROPERTY(bool hasPlasma READ hasPlasma NOTIFY loadedChanged)
    Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
    Q_PROPERTY(bool modulus READ modulus WRITE setModulus NOTIFY scaleChanged)
    Q_PROPERTY(bool globalScale READ globalScale WRITE setGlobalScale NOTIFY scaleChanged)
    Q_PROPERTY(bool topologyVisible READ topologyVisible WRITE setTopologyVisible NOTIFY topologyChanged)
    Q_PROPERTY(int scaleKind READ scaleKind NOTIFY scaleChanged)
    Q_PROPERTY(double scaleMin READ scaleMin NOTIFY scaleChanged)
    Q_PROPERTY(double scaleMax READ scaleMax NOTIFY scaleChanged)
    Q_PROPERTY(QString scaleUnit READ scaleUnit NOTIFY scaleChanged)
    Q_PROPERTY(QVariantList scaleColors READ scaleColors NOTIFY scaleChanged)
    Q_PROPERTY(double indexProgress READ indexProgress NOTIFY indexProgressChanged)
    Q_PROPERTY(QVariantList ports READ ports NOTIFY topologyChanged)
    Q_PROPERTY(bool watch READ watch WRITE setWatch NOTIFY watchChanged)
    // Контраст раскраски поля: пределы шкалы делятся на colorGain (значения за пределами —
    // крайним цветом). Только показ: данные и сама шкала значений не меняются.
    Q_PROPERTY(double colorGain READ colorGain WRITE setColorGain NOTIFY scaleChanged)

public:
    explicit FieldScene(QObject *parent = nullptr);
    ~FieldScene() override;

    QString folder() const { return m_folder; }
    void setFolder(const QString &f);
    QString file() const { return m_file; }
    void setFile(const QString &f);
    QStringList sources() const { return m_sourceNames; }
    int sourceIndex() const { return m_sourceIndex; }
    void setSourceIndex(int i);

    bool loaded() const { return m_series.count() > 0; }
    QString error() const { return m_error; }
    QString fileName() const;
    int frameCount() const { return m_series.count(); }
    int frame() const { return m_frame; }
    void setFrame(int f);
    int nT() const;
    QString timeText() const;
    int nX() const { return m_series.nX(); }
    int nY() const { return m_series.nY(); }
    QString gridText() const;
    bool hasTopology() const { return m_hasTopology; }
    bool hasPlasma() const { return m_topology.hasPlasma(); }

    int layer() const { return m_layer; }
    void setLayer(int l);
    bool modulus() const { return m_modulus; }
    void setModulus(bool on);
    bool globalScale() const { return m_globalScale; }
    void setGlobalScale(bool on);
    bool topologyVisible() const { return m_topologyVisible; }
    void setTopologyVisible(bool on);
    int scaleKind() const { return int(m_render.scale); }
    double scaleMin() const { return m_scaleMin; }
    double scaleMax() const { return m_scaleMax; }
    QString scaleUnit() const;
    QVariantList scaleColors() const;
    double indexProgress() const { return m_indexProgress; }
    QVariantList ports() const;
    bool watch() const { return m_watch; }
    void setWatch(bool on);
    double colorGain() const { return m_colorGain; }
    void setColorGain(double g);

    Q_INVOKABLE void reload();
    Q_INVOKABLE void stepFrame(int delta);
    /// Значение под точкой поля (x, y — доли 0…1 по сетке) — для подписи.
    Q_INVOKABLE double valueAt(double u, double v) const;

    const RenderData &renderData() const { return m_render; }
    /// Порты в координатах сцены (для подписей поверх картинки).
    const std::vector<TopPort> &scenePorts() const { return m_scenePorts; }
    /// Объём кэша кадров, байт.
    void setCacheBudget(qint64 bytes) { m_cacheBudget = bytes; }

Q_SIGNALS:
    void folderChanged();
    void fileChanged();
    void sourcesChanged();
    void sourceIndexChanged();
    void loadedChanged();
    void frameChanged();
    void layerChanged();
    void scaleChanged();
    void topologyChanged();
    void indexProgressChanged();
    void watchChanged();
    void renderDataChanged();

private:
    void scanFolder();
    void load(const QString &path);
    void startIndexing();
    void stopIndexing();
    std::shared_ptr<const FieldFrame> frameData(int i);
    void updateValues();
    void updateRange();
    void updateTopology();
    void pollWatch();

    QString m_folder;
    QString m_file;
    QStringList m_sourcePaths;
    QStringList m_sourceNames;
    int m_sourceIndex = -1;
    QString m_error;

    FieldSeries m_series;
    int m_frame = 0;
    std::shared_ptr<const FieldFrame> m_current;

    // Кэш кадров (LRU) в пределах бюджета.
    std::list<int> m_lru;
    std::unordered_map<int, std::shared_ptr<const FieldFrame>> m_cache;
    qint64 m_cacheBytes = 0;
    qint64 m_cacheBudget = 192LL * 1024 * 1024;

    // Размах по кадрам: заполняется при чтении кадров и фоновым проходом.
    std::vector<FieldStats> m_stats;
    std::vector<char> m_statsKnown;
    QThread *m_indexer = nullptr;
    std::shared_ptr<std::atomic<bool>> m_cancel;
    double m_indexProgress = 0.0;

    TopologyFile m_topology;
    bool m_hasTopology = false;
    std::vector<BlockBoundary> m_bounds;
    std::vector<TopPort> m_scenePorts;
    std::vector<TopPort> m_dataPorts;
    bool m_topologyVisible = true;

    int m_layer = 0;
    bool m_modulus = false;
    bool m_globalScale = true;
    double m_scaleMin = 0.0, m_scaleMax = 0.0;
    std::shared_ptr<const std::vector<float>> m_layerValues;   // ε/N/Y/B во float
    int m_layerValuesLayer = -1;

    bool m_watch = false;
    double m_colorGain = 1.0;
    QTimer m_watchTimer;
    QDateTime m_watchStamp;
    qint64 m_watchSize = -1;

    RenderData m_render;
};

} // namespace fieldview
