// Топология .tt и границы блоков — перенос логики Windows-версии FieldView
// (ссылки на исходные функции — topology.h). Числа, порядок и условия — как в оригинале.
#include "topology.h"
#include "textscan.h"

#include <QFile>

#include <cmath>
#include <cstring>

namespace fieldview {

namespace {

constexpr const char kTopId[]     = "#TMC_TopologyOutputFile FormVer2.0 2000";
constexpr const char kListBegin[] = "#BlockListBegin";
constexpr const char kListEnd[]   = "#BlockListEnd";
constexpr const char kIdNT[]      = "#TMCGROFLD_nT ";
constexpr const char kIdT[]       = "#TMCGROFLD_Time ";
constexpr const char kIdDelta[]   = "#TMCGROFLD_Delta ";
constexpr const char kIdXmin[]    = "#TMCGROFLD_Xmin ";
constexpr const char kIdYmin[]    = "#TMCGROFLD_Ymin ";
constexpr const char kIdNX[]      = "#TMCGROFLD_nX ";
constexpr const char kIdNY[]      = "#TMCGROFLD_nY ";

/// del_eol: строка до первого \r или \n.
QString delEol(const QByteArray &line)
{
    int n = 0;
    while (n < line.size() && line[n] != '\r' && line[n] != '\n')
        ++n;
    // Тексты блоков — ASCII/CP1251; имена типов и выражения в CP1251 не встречаются
    // за пределами ASCII, Latin-1 сохраняет байты как есть.
    return QString::fromLatin1(line.constData(), n);
}

bool startsWith(const QString &s, const char *prefix)
{
    return s.startsWith(QLatin1String(prefix));
}

/// Чтение сетки nY строк по nX double с пропуском 2 байт после строки (fread ch дважды).
bool readGrid(const char *&p, const char *end, int nX, int nY, std::vector<double> *out)
{
    const size_t n = size_t(nX) * size_t(nY);
    out->assign(n, 0.0);
    size_t ii = 0;
    for (int i = 0; i < nY; ++i) {
        for (int j = 0; j < nX; ++j) {
            if (end - p < 8)
                return false;
            std::memcpy(&(*out)[ii++], p, 8);
            p += 8;
        }
        // После последней строки последней сетки двух байт может не быть — как fread.
        p += (end - p >= 2) ? 2 : (end - p);
    }
    return true;
}

// --- CTmcBoundaryTpl ---------------------------------------------------------

void quad(BlockBoundary *b, double x1, double y1, double x2, double y2, double x3, double y3,
          double x4, double y4)
{
    TopQuad q;
    q.type = b->nType;
    q.x[0] = x1; q.y[0] = y1;
    q.x[1] = x2; q.y[1] = y2;
    q.x[2] = x3; q.y[2] = y3;
    q.x[3] = x4; q.y[3] = y4;
    b->quads.push_back(q);
}

void setRectStat(const TopBlock &bl, double d, BlockBoundary *b)
{
    const double xmin = bl.xMin, xmax = bl.xMax, ymin = bl.yMin, ymax = bl.yMax;
    quad(b, xmin - d / 2, ymin - d / 2, xmin - d / 2, ymax + d / 2,
         xmin + d / 2, ymax + d / 2, xmin + d / 2, ymin - d / 2);
    quad(b, xmin - d / 2, ymax - d / 2, xmin - d / 2, ymax + d / 2,
         xmax + d / 2, ymax + d / 2, xmax + d / 2, ymax - d / 2);
    quad(b, xmax - d / 2, ymin - d / 2, xmax - d / 2, ymax + d / 2,
         xmax + d / 2, ymax + d / 2, xmax + d / 2, ymin - d / 2);
    quad(b, xmin - d / 2, ymin - d / 2, xmin - d / 2, ymin + d / 2,
         xmax + d / 2, ymin + d / 2, xmax + d / 2, ymin - d / 2);
}

/// closed — замкнуть многоугольник (в оригинале — только для типа ε).
bool setPolygonStat(const TopBlock &bl, double d, bool closed, BlockBoundary *b)
{
    const int nXY = int(bl.px.size());
    const int nPoint = closed ? nXY : nXY - 1;
    if (nPoint < 2)
        return false;   // в оригинале — ошибка "memory allocation in BlockList Tpl array"
    const std::vector<double> &pX = bl.px, &pY = bl.py;
    int i;
    for (i = 0; i < nXY - 1; ++i) {
        if (std::fabs(pX[size_t(i)] - pX[size_t(i + 1)]) < std::fabs(pY[size_t(i)] - pY[size_t(i + 1)])) {
            quad(b, pX[size_t(i)] - d / 2, pY[size_t(i)] - d / 2,
                 pX[size_t(i + 1)] - d / 2, pY[size_t(i + 1)] + d / 2,
                 pX[size_t(i + 1)] + d / 2, pY[size_t(i + 1)] + d / 2,
                 pX[size_t(i)] + d / 2, pY[size_t(i)] - d / 2);
        } else {
            quad(b, pX[size_t(i)] - d / 2, pY[size_t(i)] - d / 2,
                 pX[size_t(i)] - d / 2, pY[size_t(i)] + d / 2,
                 pX[size_t(i + 1)] + d / 2, pY[size_t(i + 1)] + d / 2,
                 pX[size_t(i + 1)] + d / 2, pY[size_t(i + 1)] - d / 2);
        }
    }
    if (closed) {
        const size_t k = size_t(i);
        if (std::fabs(pX[k] - pX[0]) < std::fabs(pY[k] - pY[0])) {
            quad(b, pX[k] - d / 2, pY[k] - d / 2, pX[0] - d / 2, pY[0] + d / 2,
                 pX[0] + d / 2, pY[0] + d / 2, pX[k] + d / 2, pY[k] - d / 2);
        } else {
            quad(b, pX[k] - d / 2, pY[k] - d / 2, pX[k] - d / 2, pY[k] + d / 2,
                 pX[0] + d / 2, pY[0] + d / 2, pX[0] + d / 2, pY[0] - d / 2);
        }
    }
    return true;
}

void setInputX(const TopBlock &bl, double d, BlockBoundary *b)
{
    quad(b, -d / 2, bl.xMin - d / 2, -d / 2, bl.xMax + d / 2,
         d / 2, bl.xMax + d / 2, d / 2, bl.xMin - d / 2);
}

void setInputY(const TopBlock &bl, double d, BlockBoundary *b)
{
    quad(b, bl.xMin - d / 2, -d / 2, bl.xMin - d / 2, d / 2,
         bl.xMax + d / 2, d / 2, bl.xMax + d / 2, -d / 2);
}

/// Сегмент дуги: в оригинале вершины x1,y1 / x1,y2 / x2,y2 / x2,y1 (обе ветки одинаковы).
void arcQuad(BlockBoundary *b, double x1, double y1, double x2, double y2)
{
    quad(b, x1, y1, x1, y2, x2, y2, x2, y1);
}

/// Радиальный отрезок с полосой ±d/2 (ветки как в SetCircleStat).
void radialQuad(BlockBoundary *b, double x1, double y1, double x2, double y2, double d)
{
    if (std::fabs(x1 - x2) < std::fabs(y1 - y2))
        quad(b, x1 - d / 2, y1 - d / 2, x2 - d / 2, y2 + d / 2, x2 + d / 2, y2 + d / 2,
             x1 + d / 2, y1 - d / 2);
    else
        quad(b, x1 - d / 2, y1 - d / 2, x1 - d / 2, y1 + d / 2, x2 + d / 2, y2 + d / 2,
             x2 + d / 2, y2 - d / 2);
}

void setCircleStat(const TopBlock &bl, double d, BlockBoundary *b)
{
    const double rIn = bl.xMin, rOut = bl.xMax, f0 = bl.yMin, f1 = bl.yMax;
    // Внешняя дуга: n = (int)(Xmax*(Ymax-Ymin)/dDelta + 0.5) + 1 сегментов.
    int n = int(rOut * (f1 - f0) / d + 0.5) + 1;
    double df = (n > 1) ? (f1 - f0) / (n - 1) : 0.0;
    double f = f0;
    for (int i = 0; i < n; ++i, f += df)
        arcQuad(b, rOut * std::cos(f), rOut * std::sin(f), rOut * std::cos(f + df),
                rOut * std::sin(f + df));
    if (std::fabs(rIn) <= d)
        return;
    // Два радиальных отрезка — на углах Ymin и Ymax.
    radialQuad(b, rOut * std::cos(f0), rOut * std::sin(f0), rIn * std::cos(f0),
               rIn * std::sin(f0), d);
    radialQuad(b, rIn * std::cos(f1), rIn * std::sin(f1), rOut * std::cos(f1),
               rOut * std::sin(f1), d);
    // Внутренняя дуга.
    n = int(rIn * (f1 - f0) / d + 0.5);
    if (n < 1)
        return;
    if (n < 2) {
        arcQuad(b, rIn * std::cos(f0), rIn * std::sin(f0), rIn * std::cos(f1), rIn * std::sin(f1));
        return;
    }
    df = (f1 - f0) / (n - 1);
    f = f0;
    for (int i = 0; i < n; ++i, f += df)
        arcQuad(b, rIn * std::cos(f), rIn * std::sin(f), rIn * std::cos(f + df),
                rIn * std::sin(f + df));
}

} // namespace

bool TopologyFile::read(const QString &path, QString *error)
{
    *this = TopologyFile();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Can't open topology file %1").arg(path);
        return false;
    }
    const QByteArray all = file.readAll();
    const QString bad = QStringLiteral("Bad topology file %1").arg(path);
    LineReader r(all.constData(), all.size());
    QByteArray line;
    if (!r.next(&line) || !line.startsWith(kTopId) || !r.next(&line) || !line.startsWith(kListBegin)) {
        if (error)
            *error = bad;
        return false;
    }
    // CTmcRTH_BlockList::Load: блоки до #BlockListEnd.
    for (;;) {
        if (!r.next(&line)) {
            if (error)
                *error = bad;
            return false;
        }
        if (line.startsWith(kListEnd))
            break;
        TopBlock b;
        b.block = delEol(line);
        QByteArray l2, l3, l4, l5, nums;
        if (!r.next(&l2) || !r.next(&l3) || !r.next(&l4) || !r.next(&l5) || !r.next(&nums)) {
            if (error)
                *error = bad;
            return false;
        }
        b.eps = delEol(l2);
        b.vx = delEol(l3);
        b.vy = delEol(l4);
        b.w = delEol(l5);
        // "%lg %lg %lg %lg %lg %lg %d %d %d %d": dX0, dXmax, dXmin, dY0, dYmax, dYmin,
        // nBlock, nMemory, nType, nXY.
        int pos = 0;
        double v[6];
        for (double &x : v) {
            int nx = pos;
            x = scanDouble(nums, pos, std::nan(""), &nx);
            pos = nx;
        }
        const QList<QByteArray> parts = nums.simplified().split(' ');
        if (parts.size() < 10 || std::isnan(v[0]) || std::isnan(v[5])) {
            if (error)
                *error = bad;
            return false;
        }
        b.x0 = v[0]; b.xMax = v[1]; b.xMin = v[2];
        b.y0 = v[3]; b.yMax = v[4]; b.yMin = v[5];
        b.nBlock = parts[6].toInt();
        b.nMemory = parts[7].toInt();
        b.nType = parts[8].toInt();
        b.nXY = parts[9].toInt();
        for (int i = 0; i < b.nXY; ++i) {
            QByteArray xy;
            if (!r.next(&xy)) {
                if (error)
                    *error = bad;
                return false;
            }
            int nx = 0;
            const double x = scanDouble(xy, 0, std::nan(""), &nx);
            const double y = scanDouble(xy, nx, std::nan(""));
            if (std::isnan(x) || std::isnan(y)) {
                if (error)
                    *error = bad;
                return false;
            }
            b.px.push_back(x);
            b.py.push_back(y);
        }
        // Узел списка с nBlock == 0 оригинал не считает блоком (GetBlockNumber).
        if (b.nBlock != 0)
            m_blocks.push_back(b);
    }
    // Заголовок сетки (без Accuracy: в .tt всегда double).
    const struct { const char *id; int kind; } hdr[] = {
        {kIdNT, 0}, {kIdT, 1}, {kIdDelta, 2}, {kIdXmin, 3}, {kIdYmin, 4}, {kIdNX, 5}, {kIdNY, 6}};
    for (const auto &h : hdr) {
        if (!r.next(&line) || !line.startsWith(h.id)) {
            if (error)
                *error = bad;
            return false;
        }
        const int at = int(std::strlen(h.id));
        switch (h.kind) {
        case 0: m_nT = scanInt(line, at, m_nT); break;
        case 1: m_time = scanDouble(line, at, m_time); break;
        case 2: m_delta = scanDouble(line, at, m_delta); break;
        case 3: m_xMin = scanDouble(line, at, m_xMin); break;
        case 4: m_yMin = scanDouble(line, at, m_yMin); break;
        case 5: m_nX = scanInt(line, at, m_nX); break;
        default: m_nY = scanInt(line, at, m_nY); break;
        }
    }
    if (m_nX < 2 || m_nY < 2) {
        if (error)
            *error = QStringLiteral("Bad topology file %1 - allocation memory error nX or nY < 2, or nX*nY < 4").arg(path);
        return false;
    }
    const char *p = all.constData() + r.position();
    const char *end = all.constData() + all.size();
    if (!readGrid(p, end, m_nX, m_nY, &m_eps)) {
        if (error)
            *error = bad;
        return false;
    }
    // X-мода: ещё N, Y, B. Нет — плазмы нет (bHasPlasma = 0), ε остаётся.
    m_hasPlasma = readGrid(p, end, m_nX, m_nY, &m_n) && readGrid(p, end, m_nX, m_nY, &m_y)
                  && readGrid(p, end, m_nX, m_nY, &m_b);
    if (!m_hasPlasma) {
        m_n.clear();
        m_y.clear();
        m_b.clear();
    }
    return true;
}

const std::vector<double> &TopologyFile::grid(int type) const
{
    if (m_hasPlasma) {
        switch (type) {
        case 1: return m_n;
        case 2: return m_y;
        case 3: return m_b;
        default: break;
        }
    }
    return m_eps;
}

std::vector<BlockBoundary> buildBoundaries(const std::vector<TopBlock> &blocks, double delta,
                                           bool plasmaOutlines)
{
    std::vector<BlockBoundary> out;
    out.reserve(blocks.size());
    int inputCounter = 0;
    for (const TopBlock &bl : blocks) {
        BlockBoundary b;
        b.nType = bl.nType;
        b.nBlock = bl.nBlock;
        const QString &s = bl.block;
        // Порядок проверок — как в CTmcBoundaryTpl::SetBlockList.
        if (startsWith(s, "RECT_STAT ") || startsWith(s, "RECT_MOVE ")) {
            setRectStat(bl, delta, &b);
        } else if (startsWith(s, "FILE ")) {
            b.nType = kTypeEps;
            setRectStat(bl, delta, &b);
        } else if (startsWith(s, "POLYGON_STAT ") || startsWith(s, "POLYGON_MOVE ")) {
            setPolygonStat(bl, delta, b.nType == kTypeEps, &b);
        } else if (startsWith(s, "CIRCLE_STAT ") || startsWith(s, "CIRCLE_MOVE ")) {
            setCircleStat(bl, delta, &b);
        } else if (startsWith(s, "INPUT_X ")) {
            setInputX(bl, delta, &b);
        } else if (startsWith(s, "INPUT_Y ")) {
            setInputY(bl, delta, &b);
        } else if (plasmaOutlines) {
            // Не из оригинала: контуры плазменных блоков X-моды.
            if (startsWith(s, "RECT_STAT_N ") || startsWith(s, "RECT_STAT_B ") || startsWith(s, "RECT_STAT_Y ")) {
                b.nType = kTypePlasma;
                setRectStat(bl, delta, &b);
            } else if (startsWith(s, "POLYGON_STAT_N ") || startsWith(s, "POLYGON_STAT_B ")
                       || startsWith(s, "POLYGON_STAT_Y ")) {
                b.nType = kTypePlasma;
                setPolygonStat(bl, delta, true, &b);
            } else if (startsWith(s, "CIRCLE_STAT_N ") || startsWith(s, "CIRCLE_STAT_B ")
                       || startsWith(s, "CIRCLE_STAT_Y ")) {
                b.nType = kTypePlasma;
                setCircleStat(bl, delta, &b);
            }
        }
        // TranslateX0Y0.
        for (TopQuad &q : b.quads) {
            for (int k = 0; k < 4; ++k) {
                q.x[k] += bl.x0;
                q.y[k] += bl.y0;
            }
        }
        // ReadTpl: нумерация портов по порядку блоков.
        if (isInputType(b.nType))
            b.inputNumber = ++inputCounter;
        out.push_back(std::move(b));
    }
    return out;
}

void visibleTopology(const std::vector<BlockBoundary> &bounds, double xMin, double xMax,
                     double yMin, double yMax, double delta, const TopologyFilter &filter,
                     std::vector<TopQuad> *quads, std::vector<TopPort> *ports)
{
    quads->clear();
    ports->clear();
    const double xmin1 = xMin - 1.1 * delta;
    const double xmax1 = xMax + 1.1 * delta;
    const double ymin1 = yMin - 1.1 * delta;
    const double ymax1 = yMax + 1.1 * delta;
    for (const BlockBoundary &b : bounds) {
        const int t = b.nType;
        const bool shown = (t == kTypeEps && filter.eps) || (isInputType(t) && filter.input)
                           || (t == kTypeAbsorber && filter.absorber) || (t == kTypeMetal && filter.metal)
                           || (t == kTypeMagnetic && filter.magnetic) || (t == kTypePlasma && filter.plasma);
        if (!shown || b.quads.empty())
            continue;
        if (isInputType(t)) {
            const TopQuad &q = b.quads.front();
            TopPort p;
            p.number = b.inputNumber;
            p.x = (q.x[0] + q.x[1] + q.x[2] + q.x[3]) / 4.;
            p.y = (q.y[0] + q.y[1] + q.y[2] + q.y[3]) / 4.;
            ports->push_back(p);
        }
        for (const TopQuad &q : b.quads) {
            bool in = false;
            for (int k = 0; k < 4 && !in; ++k)
                in = q.x[k] < xmax1 && q.x[k] > xmin1 && q.y[k] < ymax1 && q.y[k] > ymin1;
            if (in)
                quads->push_back(q);
        }
    }
}

} // namespace fieldview
