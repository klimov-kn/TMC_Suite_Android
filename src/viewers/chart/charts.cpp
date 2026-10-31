#include "charts.h"

#include "../data/dnpattern.h"
#include "../data/fieldfile.h"
#include "../data/signalfile.h"
#include "../data/smatrixfile.h"
#include "../viewers.h"

#include <algorithm>
#include <cfloat>
#include <limits>
#include <cmath>

namespace viewers {

// ============================================================================
// S-матрица (TMCGROUT)
// ============================================================================

namespace {

struct SDoc : ChartBase::Doc
{
    struct File { QString name; SMatrixFile s; bool compared = false; };
    std::vector<File> files;
};

// Виды оси Y (порядок в списке экрана) -> TMC_GR_TYPE_* (Include/Tmcgrviw.h).
enum SMode { SModeAbs = 0, SModeLoss, SModeVswr, SModePhaseDeg, SModePhaseRad };

} // namespace

SMatrixChart::SMatrixChart(QQuickItem *parent)
    : ChartBase(parent)
{
    retranslate();
}

void SMatrixChart::retranslate()
{
    setModes({ QStringLiteral("|S|"), tr("L, дБ"), tr("КСВН"), tr("Фаза, °"), tr("Фаза, рад") });
}

ChartBase::Loader SMatrixChart::loader() const
{
    return [](const std::vector<InputFile> &in, QString *error) -> std::shared_ptr<Doc> {
        auto doc = std::make_shared<SDoc>();
        for (const InputFile &f : in) {
            SDoc::File one{ f.name, {}, f.compared };
            if (!one.s.parse(f.bytes)) {
                if (doc->files.empty()) {   // открытый файл не читается — ошибка
                    *error = f.name + QStringLiteral(": ") + one.s.error();
                    return nullptr;
                }
                continue;
            }
            doc->files.push_back(std::move(one));
        }
        return doc;
    };
}

void SMatrixChart::rebuild()
{
    const auto *doc = static_cast<const SDoc *>(m_doc.get());
    const QString yTitles[] = { QStringLiteral("|S|"), tr("L, дБ"), tr("КСВН"), tr("φ, °"), tr("φ, рад") };
    setAxisTitles(tr("f, ГГц"), yTitles[m_mode]);
    int color = 0;
    for (size_t fi = 0; fi < doc->files.size(); ++fi) {
        const SMatrixFile &sf = doc->files[fi].s;
        const SMatrixFile::FreqPoint &p0 = sf.points().front();
        bool singleMode = true;
        for (short m : p0.modes)
            singleMode = singleMode && m == 1;
        for (int in1 = 1; in1 <= p0.num; ++in1) {
            for (int mod1 = 1; mod1 <= p0.modes[size_t(in1 - 1)]; ++mod1) {
                for (int in2 = 1; in2 <= p0.num; ++in2) {
                    for (int mod2 = 1; mod2 <= p0.modes[size_t(in2 - 1)]; ++mod2) {
                        int a = in1, b = mod1, c = in2, d = mod2;
                        std::vector<double> f;
                        std::vector<Cplx> v;
                        if (!sf.element(a, b, c, d, &f, &v))
                            continue;
                        // Ядро пишет FLT_MAX в элементы, которые не считались (возбуждён не
                        // каждый вход) — такой ряд не рисуется и помечается «нет данных».
                        bool computed = false;
                        for (const Cplx &z : v)
                            computed = computed || (std::fabs(z.x) < FLT_MAX && std::fabs(z.y) < FLT_MAX);
                        Series s;
                        const QString sep = p0.num >= 10 ? QStringLiteral(",") : QString();
                        s.name = singleMode ? QStringLiteral("S%1%2%3").arg(in1).arg(sep).arg(in2)
                                            : QStringLiteral("S%1%2%3 [%4,%5]").arg(in1).arg(sep).arg(in2).arg(mod1).arg(mod2);
                        s.group = doc->files[fi].name;
                        s.key = s.group + QLatin1Char('|') + s.name;
                        if (!computed)
                            s.name += tr(" — нет данных");
                        s.colorIndex = color++;
                        s.compared = doc->files[fi].compared;
                        s.defaultVisible = computed && (fi == 0 || s.compared) && in2 == 1 && in1 <= 2
                                           && mod1 == 1 && mod2 == 1;
                        s.x.resize(f.size());
                        s.y.resize(f.size());
                        // CTMCGROUTView::PrepareDoubleGraph (Tmcgrout/TMCGROUTView.cpp), без выражений.
                        for (size_t j = 0; j < f.size(); ++j) {
                            s.x[j] = f[j] / (1.e+9);                                   // TMC_GR_TYPE_FGHz
                            if (!(std::fabs(v[j].x) < FLT_MAX && std::fabs(v[j].y) < FLT_MAX)) {
                                s.y[j] = std::numeric_limits<double>::quiet_NaN();
                                continue;
                            }
                            switch (m_mode) {
                            case SModeLoss:                                            // TMC_GR_TYPE_L
                                s.y[j] = -20. * std::log10(tmc::cabsv(v[j]));
                                break;
                            case SModeVswr: {                                          // TMC_GR_TYPE_WSVR
                                double r11 = tmc::cabsv(v[j]);
                                if (std::fabs(r11 - 1.) < 0.0001)
                                    r11 = 0.9999;
                                s.y[j] = (1. + r11) / (1. - r11);
                                break;
                            }
                            case SModePhaseDeg:                                        // TMC_GR_TYPE_SFD
                                s.y[j] = 180. / (3.141592653589) * tmc::cphase(v[j]);
                                break;
                            case SModePhaseRad:                                        // TMC_GR_TYPE_SFR
                                s.y[j] = tmc::cphase(v[j]);
                                break;
                            case SModeAbs:                                             // TMC_GR_TYPE_SM
                            default:
                                s.y[j] = tmc::cabsv(v[j]);
                                break;
                            }
                        }
                        m_series.push_back(std::move(s));
                    }
                }
            }
        }
    }
    const SMatrixFile &first = doc->files.front().s;
    const auto &pts = first.points();
    QString info = tr("частот: %1 · входов %2").arg(pts.size()).arg(first.inputs());
    if (pts.size() == 1)
        info += tr(" · %1 ГГц").arg(fmt(pts.front().f / 1.e+9));
    else
        info += tr(" · %1…%2 ГГц").arg(fmt(pts.front().f / 1.e+9), fmt(pts.back().f / 1.e+9));
    setInfo(info);
}

QStringList SMatrixChart::pointText(const Series &s, size_t i) const
{
    const QString yNames[] = { QStringLiteral("|S|"), QStringLiteral("L"), tr("КСВН"), QStringLiteral("φ"),
                               QStringLiteral("φ") };
    const QString yUnits[] = { QString(), tr(" дБ"), QString(), QStringLiteral("°"), tr(" рад") };
    return { tr("f = %1 ГГц").arg(fmt(s.x[i], 7)),
             QStringLiteral("%1 = %2%3").arg(yNames[m_mode], fmt(s.y[i]), yUnits[m_mode]) };
}

// ============================================================================
// ДН (TMC_DN)
// ============================================================================

namespace {

struct DnDoc : ChartBase::Doc
{
    struct File { QString name; DnPattern dn; bool compared = false; };
    std::vector<File> files;
};

constexpr double kPolarDbFloor = -40.0;   // полярные в дБ: центр круга — −40 дБ

} // namespace

PatternChart::PatternChart(QQuickItem *parent)
    : ChartBase(parent)
{
    retranslate();
}

void PatternChart::retranslate()
{
    setModes({ tr("Линейно"), tr("дБ") });
}

ChartBase::Loader PatternChart::loader() const
{
    return [](const std::vector<InputFile> &in, QString *error) -> std::shared_ptr<Doc> {
        auto doc = std::make_shared<DnDoc>();
        for (const InputFile &f : in) {
            DnDoc::File one{ f.name, {}, f.compared };
            if (!one.dn.compute(f.bytes)) {
                if (doc->files.empty()) {
                    *error = f.name + QStringLiteral(": ") + one.dn.error();
                    return nullptr;
                }
                continue;
            }
            doc->files.push_back(std::move(one));
        }
        return doc;
    };
}

void PatternChart::rebuild()
{
    const auto *doc = static_cast<const DnDoc *>(m_doc.get());
    const bool db = m_mode == 1;
    setAxisTitles(QStringLiteral("φ, °"), db ? tr("F(φ), дБ") : QStringLiteral("F(φ)"));
    int color = 0;
    for (size_t fi = 0; fi < doc->files.size(); ++fi) {
        const DnPattern &dn = doc->files[fi].dn;
        Series s;
        s.name = doc->files[fi].name;
        s.group = s.name;
        s.key = s.name;
        s.colorIndex = color++;
        s.compared = doc->files[fi].compared;
        s.defaultVisible = fi == 0 || s.compared;
        const size_t n = dn.angles().size();
        s.x.resize(n);
        s.y.resize(n);
        for (size_t i = 0; i < n; ++i) {
            // Угол: в полярных — радианы, в декартовых — градусы (TMC_DN «Angle degree»).
            s.x[i] = m_polar ? dn.angles()[i] : dn.angles()[i] / 3.141592653589 * 180.;
            s.y[i] = dn.value(i, false, db);   // нормированная ДН (PHAS_R / PHAS_G)
        }
        m_series.push_back(std::move(s));
    }
    // Сводка — как строка состояния TMC_DN (TMCGROUTView.cpp, PutStatistics1):
    // «Knd =%7.4lgdB; Emitted Power =%5.2lg;» или «…; Kip =%5.2lg;».
    const DnPattern &first = doc->files.front().dn;
    QString info = tr("КНД %1 дБ · ").arg(fmt(first.kndDb(), 4));
    info += first.kipIsEmittedPower() ? tr("P изл. %1").arg(fmt(first.kip(), 2))
                                      : tr("КИП %1").arg(fmt(first.kip(), 2));
    info += tr(" · N %1 · %2 ГГц").arg(first.emitters()).arg(fmt(first.freqGHz()));
    setInfo(info);
}

double PatternChart::yAutoFloorSpan() const { return m_mode == 1 ? 60.0 : 0.0; }

double PatternChart::polarRadius(double y) const
{
    if (m_mode == 1)
        return std::isfinite(y) ? std::max(0.0, (y - kPolarDbFloor) / -kPolarDbFloor) : 0.0;
    return y;
}

QString PatternChart::polarRingLabel(double r) const
{
    if (m_mode == 1)
        return fmt(kPolarDbFloor * (1.0 - r), 3) + tr(" дБ");
    return fmt(r, 3);
}

QStringList PatternChart::pointText(const Series &s, size_t i) const
{
    const double deg = m_polar ? s.x[i] / 3.141592653589 * 180. : s.x[i];
    return { QStringLiteral("φ = %1°").arg(fmt(deg, 5)),
             m_mode == 1 ? tr("F = %1 дБ").arg(fmt(s.y[i], 5))
                         : QStringLiteral("F = %1").arg(fmt(s.y[i], 6)) };
}

// ============================================================================
// Сигналы (TMCROS)
// ============================================================================

namespace {

struct SigDoc : ChartBase::Doc
{
    struct File { QString name; SignalFile t; bool compared = false; };
    std::vector<File> files;
};

const SignalFile::YType kSigModes[] = { SignalFile::Signal, SignalFile::Amplitude, SignalFile::LossDb,
                                        SignalFile::Vswr };

} // namespace

SignalChart::SignalChart(QQuickItem *parent)
    : ChartBase(parent)
{
    retranslate();
}

void SignalChart::retranslate()
{
    setModes({ tr("Сигнал"), tr("Амплитуда"), tr("L, дБ"), tr("КСВН") });
}

ChartBase::Loader SignalChart::loader() const
{
    return [](const std::vector<InputFile> &in, QString *error) -> std::shared_ptr<Doc> {
        auto doc = std::make_shared<SigDoc>();
        for (const InputFile &f : in) {
            SigDoc::File one{ f.name, {}, f.compared };
            if (!one.t.parse(f.bytes)) {
                if (doc->files.empty()) {
                    *error = f.name + QStringLiteral(": ") + one.t.error();
                    return nullptr;
                }
                continue;
            }
            doc->files.push_back(std::move(one));
        }
        return doc;
    };
}

void SignalChart::rebuild()
{
    const auto *doc = static_cast<const SigDoc *>(m_doc.get());
    const QString yTitles[] = { QStringLiteral("U"), QStringLiteral("|U|"), tr("L, дБ"), tr("КСВН") };
    setAxisTitles(tr("t, нс"), yTitles[m_mode]);
    const SignalFile::YType type = kSigModes[m_mode];
    int color = 0;
    for (size_t fi = 0; fi < doc->files.size(); ++fi) {
        const SignalFile &t = doc->files[fi].t;
        std::vector<double> x(t.time().size());
        for (size_t j = 0; j < x.size(); ++j)
            x[j] = t.time()[j] * (1.e+9);   // TMC_GROTS_TYPE_ns
        for (size_t k = 0; k < t.ports().size(); ++k) {
            const SignalFile::Port &port = t.ports()[k];
            for (int which = 0; which < 2; ++which) {
                Series s;
                s.name = tr("Порт %1 (бл. %2) · %3").arg(k + 1).arg(port.block)
                             .arg(which == 0 ? QStringLiteral("Up") : QStringLiteral("Uo"));
                s.group = doc->files[fi].name;
                s.key = s.group + QLatin1Char('|') + s.name;
                s.colorIndex = color++;
                s.compared = doc->files[fi].compared;
                s.defaultVisible = fi == 0 || s.compared;
                s.x = x;
                s.y = SignalFile::transform(which == 0 ? port.up : port.uo, type);
                m_series.push_back(std::move(s));
            }
        }
    }
    const SignalFile &first = doc->files.front().t;
    setInfo(tr("%1 тактов · портов %2 · f = %3 ГГц")
                .arg(first.time().size()).arg(first.ports().size()).arg(fmt(first.freq() / 1.e+9)));
}

QStringList SignalChart::pointText(const Series &s, size_t i) const
{
    const QString yNames[] = { QStringLiteral("U"), QStringLiteral("|U|"), QStringLiteral("L"), tr("КСВН") };
    return { s.group,
             tr("t = %1 нс · такт %2").arg(fmt(s.x[i], 6)).arg(i),
             QStringLiteral("%1 = %2%3").arg(yNames[m_mode], fmt(s.y[i]), m_mode == 2 ? tr(" дБ") : QString()) };
}

// ============================================================================

QString ViewerTools::chartKind(const QString &fileName) const { return viewers::chartKind(fileName); }

QString ViewerTools::fieldSummary(const QString &folder, const QString &fileName) const
{
    FieldFile f;
    if (!f.parse(fileReader()(folder, fileName)))
        return {};
    const double step = f.delta() / (f.longUnitValue() > 0 ? f.longUnitValue() : 1.0);
    return tr("Поле %1 × %2 узлов, шаг %3 %4").arg(f.nx()).arg(f.ny())
        .arg(localizeNumber(QString::number(step, 'g', 4)), f.longUnit());
}

} // namespace viewers
