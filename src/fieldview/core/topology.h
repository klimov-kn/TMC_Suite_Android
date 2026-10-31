// Файл топологии <имя>.tt (TOPOLOGY ядер) и границы блоков для показа.
//
// Перенос без изменений логики из Windows-версии FieldView:
//   * CTmcBlockTpl::ReadData (src/win_src/fieldview/TmcBlockTpl.cpp) и
//     CTmcRTH_BlockList::Load (src/win_src/libs/TMCIndan/TmcRTH_BolckList.cpp) — чтение;
//   * CTmcBoundaryTpl::SetBlockList / SetRectStat / SetPolygonStat / SetCircleStat /
//     SetInputX / SetInputY / TranslateX0Y0 (TmcBoundaryTpl.cpp) — четырёхугольники границ;
//   * CFldViewView::ReadTpl, CalcnTopPolygon, PrepareBoundaryTpl (FldViewView.cpp) —
//     нумерация портов («Inp N»), отбор видимых четырёхугольников, центры портов.
//
// Формат: строка-признак, список блоков (#BlockListBegin … #BlockListEnd), заголовок
// (nT, Time, Delta, Xmin, Ymin, nX, nY), сетка ε (double), у X-моды ещё N, Y, B.
#pragma once

#include <QString>

#include <vector>

namespace fieldview {

// Типы блоков — src/win_src/Include/Tmcgrviw.h (CTMCRTH_BLCKNTYPE_*).
enum BlockType : int {
    kTypeEps = 0,
    kTypeAbsorber = 100,
    kTypeMetal = 101,
    kTypeInpXLeft = 102,
    kTypeInpXRight = 103,
    kTypeInpYTop = 104,
    kTypeInpYBot = 105,
    kTypeMagnetic = 107,
    /// Не из оригинала: контур плазменного блока (RECT_STAT_N/_B/_Y …). Windows такие
    /// блоки пропускает; здесь их контур рисуется цветом «плазма» (style-guide).
    kTypePlasma = 1000,
};

inline bool isInputType(int t)
{
    return t == kTypeInpXLeft || t == kTypeInpXRight || t == kTypeInpYTop || t == kTypeInpYBot;
}

/// Запись блока (CTmcRTH_BlockList::Save1 / Load).
struct TopBlock
{
    QString block;     // "RECT_STAT ", "INPUT_X " …
    QString eps;       // выражение ε / тип ("METAL")
    QString vx, vy, w;
    double x0 = 0, xMax = 0, xMin = 0, y0 = 0, yMax = 0, yMin = 0;
    int nBlock = 0, nMemory = 0, nType = 0, nXY = 0;
    std::vector<double> px, py;
};

/// Четырёхугольник границы (вершины 1..4, в координатах задания, м).
struct TopQuad
{
    int type = kTypeMetal;
    double x[4] = {0, 0, 0, 0};
    double y[4] = {0, 0, 0, 0};
};

/// Порт (вход): номер «Inp N» и центр первого четырёхугольника.
struct TopPort
{
    int number = 0;
    double x = 0, y = 0;
};

/// Границы одного блока (CTmcBoundaryTpl).
struct BlockBoundary
{
    int nType = kTypeMetal;
    int nBlock = 0;
    int inputNumber = 0;   // 1, 2 … для портов (ReadTpl), иначе 0
    std::vector<TopQuad> quads;
};

class TopologyFile
{
public:
    bool read(const QString &path, QString *error);

    const std::vector<TopBlock> &blocks() const { return m_blocks; }
    int nT() const { return m_nT; }
    double time() const { return m_time; }
    double delta() const { return m_delta; }
    double xMin() const { return m_xMin; }
    double yMin() const { return m_yMin; }
    int nX() const { return m_nX; }
    int nY() const { return m_nY; }
    bool hasPlasma() const { return m_hasPlasma; }
    /// Сетка по типу, как GetpdSurfaceByType: 0 — ε, 1 — N, 2 — Y, 3 — B
    /// (нет плазмы — всегда ε).
    const std::vector<double> &grid(int type) const;

private:
    std::vector<TopBlock> m_blocks;
    int m_nT = 0;
    double m_time = 0;
    double m_delta = 1;
    double m_xMin = 0, m_yMin = 0;
    int m_nX = 0, m_nY = 0;
    std::vector<double> m_eps, m_n, m_y, m_b;
    bool m_hasPlasma = false;
};

/// Границы всех блоков (ReadTpl + SetBlockList). delta — шаг сетки файла поля.
/// plasmaOutlines — добавить контуры плазменных блоков (не из оригинала).
std::vector<BlockBoundary> buildBoundaries(const std::vector<TopBlock> &blocks, double delta,
                                           bool plasmaOutlines);

/// Флаги показа типов (bDrawFlagEps, …Input, …Absorber, …Metal, …Magnetic).
struct TopologyFilter
{
    bool eps = true, input = true, absorber = true, metal = true, magnetic = true;
    bool plasma = true;
};

/// Видимые четырёхугольники и центры портов (CalcnTopPolygon + PrepareBoundaryTpl).
/// xMin..xMax, yMin..yMax — показываемая область поля (у Windows 0…(nX-1)·Delta).
void visibleTopology(const std::vector<BlockBoundary> &bounds, double xMin, double xMax,
                     double yMin, double yMax, double delta, const TopologyFilter &filter,
                     std::vector<TopQuad> *quads, std::vector<TopPort> *ports);

} // namespace fieldview
