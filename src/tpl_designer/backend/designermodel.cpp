/// @file designermodel.cpp
/// @brief designermodel.cpp
/// @see https://www.tamic.ru/docs/platforms/android.html#designer

#include "designermodel.h"

#include "cp1251.h"
#include "emitter.h"
#include "ramestimate.h"
#include "sigfile.h"
#include "taskfiles.h"
#include "tplreader.h"

#include <QCoreApplication>
#include <QEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMetaObject>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

using namespace tpl;

namespace {

/// «5 частот» на языке интерфейса: русские три формы (plural из ядра) или английская
/// пара «1 task / 2 tasks» (слова уже переведены).
QString uiPlural(int n, const QString &one, const QString &few, const QString &many)
{
    if (QLocale().language() == QLocale::Russian)
        return plural(n, one, few, many);
    return QStringLiteral("%1 %2").arg(n).arg(n == 1 ? one : many);
}

/// Глубина отмены — как в оригинале (mainwindow.cpp, kUndoDepth).
constexpr int kUndoDepth = 120;

bool isContentUri(const QString &folder)
{
    return folder.startsWith(QLatin1String("content:"), Qt::CaseInsensitive);
}

QString materialKey(Material m)
{
    switch (m) {
    case Material::Metal:      return QStringLiteral("metal");
    case Material::Magnetic:   return QStringLiteral("magnetic");
    case Material::Absorber:   return QStringLiteral("absorber");
    case Material::Dielectric: return QStringLiteral("dielectric");
    }
    return QStringLiteral("metal");
}

Material materialFromKey(const QString &k)
{
    if (k == QLatin1String("magnetic"))   return Material::Magnetic;
    if (k == QLatin1String("absorber"))   return Material::Absorber;
    if (k == QLatin1String("dielectric")) return Material::Dielectric;
    return Material::Metal;
}

QString kindKey(ShapeKind k)
{
    switch (k) {
    case ShapeKind::Rectangle: return QStringLiteral("rect");
    case ShapeKind::Circle:    return QStringLiteral("circle");
    case ShapeKind::Polygon:   return QStringLiteral("polygon");
    case ShapeKind::Polyline:  return QStringLiteral("polyline");
    }
    return QStringLiteral("rect");
}

QString sideKey(PortSide s)
{
    switch (s) {
    case PortSide::Left:   return QStringLiteral("left");
    case PortSide::Right:  return QStringLiteral("right");
    case PortSide::Bottom: return QStringLiteral("bottom");
    case PortSide::Top:    return QStringLiteral("top");
    }
    return QStringLiteral("left");
}

PortSide sideFromKey(const QString &k)
{
    if (k == QLatin1String("right"))  return PortSide::Right;
    if (k == QLatin1String("bottom")) return PortSide::Bottom;
    if (k == QLatin1String("top"))    return PortSide::Top;
    return PortSide::Left;
}

QString sideText(PortSide s)
{
    switch (s) {
    case PortSide::Left:   return QCoreApplication::translate("TplDesigner", "левая");
    case PortSide::Right:  return QCoreApplication::translate("TplDesigner", "правая");
    case PortSide::Bottom: return QCoreApplication::translate("TplDesigner", "нижняя");
    case PortSide::Top:    return QCoreApplication::translate("TplDesigner", "верхняя");
    }
    return {};
}

/// Какие плазменные величины несёт фигура — коротко (mainwindow.cpp, plasmaShort).
QString plasmaShort(const Plasma &pl)
{
    QStringList parts;
    if (pl.hasN) parts << QStringLiteral("N");
    if (pl.hasB) parts << QStringLiteral("B");
    if (pl.hasY) parts << QStringLiteral("Y");
    return parts.join(QStringLiteral("+"));
}

/// То же с числами — для сводки (mainwindow.cpp, plasmaFull).
QString plasmaFull(const Plasma &pl)
{
    QStringList parts;
    if (pl.hasN) {
        parts << (pl.nFile.isEmpty()
                      ? QCoreApplication::translate("TplDesigner", "N = %1 м^-3").arg(plasmaFieldText(pl.nFormula, pl.n))
                      : QCoreApplication::translate("TplDesigner", "профиль N из файла %1").arg(pl.nFile));
    }
    if (pl.hasB) parts << QCoreApplication::translate("TplDesigner", "B = %1 Тл").arg(plasmaFieldText(pl.bFormula, pl.b));
    if (pl.hasY) parts << QStringLiteral("Y = %1").arg(plasmaFieldText(pl.yFormula, pl.y));
    return parts.join(QStringLiteral(", "));
}

/// Что за фигура — строка списка объектов (mainwindow.cpp, refreshLegend).
QString shapeWhat(const Shape &s)
{
    QString what;
    if (s.plasma.any())
        what = QCoreApplication::translate("TplDesigner", "плазма: %1").arg(plasmaShort(s.plasma));
    else if (s.epsProfile.readsFile())
        what = QCoreApplication::translate("TplDesigner", "eps из файла %1.eps").arg(epsProfileFile(s));
    else if (s.hasEpsProfile())
        what = QCoreApplication::translate("TplDesigner", "eps по формуле");
    else if (s.material == Material::Dielectric)
        what = QStringLiteral("eps = %1").arg(s.eps);
    else
        what = materialName(s.material);
    if (s.kind == ShapeKind::Rectangle && !s.hasUniformEdges())
        what += QCoreApplication::translate("TplDesigner", ", стороны разные");
    return what;
}

QString kindText(const Shape &s)
{
    switch (s.kind) {
    case ShapeKind::Rectangle: return QCoreApplication::translate("TplDesigner", "прямоугольник");
    case ShapeKind::Circle:    return QCoreApplication::translate("TplDesigner", "круг/сектор");
    case ShapeKind::Polygon:   return QCoreApplication::translate("TplDesigner", "контур, вершин %1").arg(s.points.size());
    case ShapeKind::Polyline:  return QCoreApplication::translate("TplDesigner", "линия, точек %1").arg(s.points.size());
    }
    return {};
}

/// Следующее имя вида «Линия 3» — по своему виду фигур (geometryview.cpp, nextName).
QString nextName(const QStringList &existing, const QString &base)
{
    const QRegularExpression re(QStringLiteral("^%1 (\\d+)$")
                                    .arg(QRegularExpression::escape(base)));
    int best = 0;
    for (const QString &n : existing) {
        const QRegularExpressionMatch m = re.match(n);
        if (m.hasMatch())
            best = std::max(best, m.captured(1).toInt());
    }
    return QStringLiteral("%1 %2").arg(base).arg(best + 1);
}

/// Повторы замечаний схлопываются (issuedialog.cpp, collapseIssues).
QStringList collapseIssues(const QStringList &items)
{
    // «строка 12» / «line 12» — замечания на любом языке интерфейса.
    static const QRegularExpression lineRe(QStringLiteral("(строка|line) \\d+"));
    QStringList out;
    QHash<QString, int> at;
    QVector<int> count;
    for (const QString &s : items) {
        QString key = s;
        key.replace(lineRe, QStringLiteral("\\1"));
        const auto it = at.constFind(key);
        if (it == at.constEnd()) {
            at.insert(key, int(out.size()));
            count << 1;
            out << s;
        } else {
            ++count[*it];
        }
    }
    for (int i = 0; i < out.size(); ++i) {
        if (count.at(i) > 1) {
            out[i] += QCoreApplication::translate("TplDesigner", " — и ещё %1 раз(а) то же самое дальше по файлу")
                          .arg(count.at(i) - 1);
        }
    }
    return out;
}

double toDouble(const QVariant &v, bool *ok)
{
    if (v.typeId() == QMetaType::QString) {
        QString t = v.toString().trimmed();
        t.replace(QLatin1Char(','), QLatin1Char('.'));
        return t.toDouble(ok);
    }
    return v.toDouble(ok);
}

QString pointsText(const QVector<QPointF> &pts)
{
    QStringList lines;
    for (const QPointF &p : pts)
        lines << QStringLiteral("%1; %2").arg(TplDesignerModel::num(p.x()),
                                               TplDesignerModel::num(p.y()));
    return lines.join(QLatin1Char('\n'));
}

QVector<QPointF> parsePoints(const QString &text, bool *ok)
{
    QVector<QPointF> out;
    *ok = true;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        const QStringList xy = line.split(QRegularExpression(QStringLiteral("[;\\s]+")),
                                          Qt::SkipEmptyParts);
        if (xy.size() != 2) {
            *ok = false;
            return {};
        }
        bool a = false, b = false;
        QString xs = xy.at(0), ys = xy.at(1);
        xs.replace(QLatin1Char(','), QLatin1Char('.'));
        ys.replace(QLatin1Char(','), QLatin1Char('.'));
        const double x = xs.toDouble(&a), y = ys.toDouble(&b);
        if (!a || !b) {
            *ok = false;
            return {};
        }
        out << QPointF(x, y);
    }
    return out;
}

} // namespace

// ============================================================================

TplDesignerModel::TplDesignerModel(QObject *parent)
    : QObject(parent)
{
    newProject();
    QCoreApplication::instance()->installEventFilter(this);
}

bool TplDesignerModel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        refresh();
    return QObject::eventFilter(watched, event);
}

void TplDesignerModel::newProject()
{
    // Как setProject(Project{}) оригинала: чистый холст, имена файлов по умолчанию.
    m_project = Project{};
    m_project.output.signalFile = QStringLiteral("signal");
    m_project.output.sMatrixFile = QStringLiteral("smatrix");
    m_project.output.patternFile = QStringLiteral("pattern");
    m_project.name = QStringLiteral("task");
    m_project.freqs = {4.0};
    m_companions.clear();
    m_undo.clear();
    m_redo.clear();
    m_selectedId = -1;
    m_multi.clear();
    pushToForm();
    setNote(QString());
    refresh();
}

void TplDesignerModel::setTaskFolder(const QString &folder)
{
    if (folder == m_taskFolder)
        return;
    m_taskFolder = folder;
    emit taskFolderChanged();
}

void TplDesignerModel::setExporter(QObject *o)
{
    if (o == m_exporter)
        return;
    m_exporter = o;
    emit exporterChanged();
}

void TplDesignerModel::setNote(const QString &n)
{
    if (n == m_note)
        return;
    m_note = n;
    emit noteChanged();
}

// ------------------------------------------------------------------ правки

void TplDesignerModel::snapshot()
{
    m_undo << m_project;
    if (m_undo.size() > kUndoDepth)
        m_undo.removeFirst();
    m_redo.clear();
}

void TplDesignerModel::beginEdit()
{
    if (m_editing)
        return;
    m_editing = true;
    snapshot();
}

void TplDesignerModel::endEdit()
{
    m_editing = false;
    refresh();
}

void TplDesignerModel::touch()
{
    // Во время протяжки пересчитываем только то, что видно на холсте; сводку и
    // предпросмотр — по отпусканию пальца (endEdit).
    ++m_revision;
    emit changed();
}

void TplDesignerModel::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo << m_project;
    m_project = m_undo.takeLast();
    pushToForm();
    refresh();
}

void TplDesignerModel::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo << m_project;
    m_project = m_redo.takeLast();
    pushToForm();
    refresh();
}

/// Поля частоты по заданию (mainwindow.cpp, pushToForm).
void TplDesignerModel::pushToForm()
{
    if (m_project.freqs.size() > 1) {
        m_freqRange = true;
        m_freqStart = m_project.freqs.first();
        m_freqEnd = m_project.freqs.last();
        const double st = (m_project.freqs.last() - m_project.freqs.first())
                          / (m_project.freqs.size() - 1);
        if (st > 0.0)
            m_freqStep = st;
    } else {
        m_freqRange = false;
        if (!m_project.freqs.isEmpty())
            m_freqSingle = m_project.freqs.first();
    }
}

/// Сетка частот (mainwindow.cpp, collectFreqs) — тем же циклом с допуском в полшага.
QVector<double> TplDesignerModel::collectFreqs() const
{
    QVector<double> f;
    if (!m_freqRange) {
        f << m_freqSingle;
        return f;
    }
    const double a = m_freqStart, b = m_freqEnd, s = m_freqStep;
    if (s <= 0.0 || b < a)
        return {a};
    for (double x = a; x <= b + s * 0.5; x += s)
        f << x;
    return f;
}

/// Самый мелкий шаг, при котором диапазон укладывается в 99 частот
/// (mainwindow.cpp, stepForMaxFreqs).
double TplDesignerModel::stepForMaxFreqs() const
{
    const double a = m_freqStart, b = m_freqEnd;
    const auto count = [&](double s) {
        int c = 0;
        for (double x = a; x <= b + s * 0.5 && c <= tpl::kMaxFreqs; x += s)
            ++c;
        return c;
    };
    double s = std::ceil((b - a) / double(tpl::kMaxFreqs - 1) * 1000.0) / 1000.0;
    s = std::max(s, 0.001);
    while (count(s) > tpl::kMaxFreqs)
        s += 0.001;
    return s;
}

/// Окно снятия едет за временем счёта (mainwindow.cpp, followTimeWindow):
/// последняя четверть счёта, но не меньше 3 нс; трогаем только вышедшее за счёт.
void TplDesignerModel::followTimeWindow()
{
    const double t = m_project.timeMax;
    if (t <= 0.0)
        return;
    const double win = std::max(3.0, t * 0.25);
    const auto fix = [&](double *lo, double *hi) {
        if (*hi <= t && *lo < *hi)
            return;
        *lo = std::max(0.0, t - win);
        *hi = t;
    };
    fix(&m_project.output.sMatrixTMin, &m_project.output.sMatrixTMax);
    fix(&m_project.output.patternTMin, &m_project.output.patternTMax);
}

void TplDesignerModel::setSetting(const QString &key, const QVariant &value)
{
    if (key == QLatin1String("sWide")) {
        snapshot();
        m_project.output.sMatrixLimitWideband = value.toBool();
        refresh();
        return;
    }
    bool ok = true;
    const double d = toDouble(value, &ok);
    const bool numeric = ok;
    const auto needNumber = [&]() {
        if (!numeric)
            setNote(QCoreApplication::translate("TplDesigner", "Не число: «%1»").arg(value.toString()));
        return numeric;
    };

    snapshot();
    OutputSettings &o = m_project.output;
    if (key == QLatin1String("name")) {
        m_project.name = value.toString().trimmed();
    } else if (key == QLatin1String("description")) {
        m_project.description = value.toString();
    } else if (key == QLatin1String("kernel")) {
        m_project.kernel = value.toString().compare(QLatin1String("X"), Qt::CaseInsensitive) == 0
                               ? Kernel::X : Kernel::H;
    } else if (key == QLatin1String("xMin")) { if (needNumber()) m_project.box.xMin = d;
    } else if (key == QLatin1String("xMax")) { if (needNumber()) m_project.box.xMax = d;
    } else if (key == QLatin1String("yMin")) { if (needNumber()) m_project.box.yMin = d;
    } else if (key == QLatin1String("yMax")) { if (needNumber()) m_project.box.yMax = d;
    } else if (key == QLatin1String("delta")) { if (needNumber()) m_project.delta = d;
    } else if (key == QLatin1String("timeMax")) { if (needNumber()) m_project.timeMax = d;
    } else if (key == QLatin1String("freqRange")) { m_freqRange = value.toBool();
    } else if (key == QLatin1String("freqSingle")) { if (needNumber()) m_freqSingle = d;
    } else if (key == QLatin1String("freqStart")) { if (needNumber()) m_freqStart = d;
    } else if (key == QLatin1String("freqEnd")) { if (needNumber()) m_freqEnd = d;
    } else if (key == QLatin1String("freqStep")) { if (needNumber()) m_freqStep = d;
    } else if (key == QLatin1String("useMaxStep")) { m_freqStep = stepForMaxFreqs();
    } else if (key == QLatin1String("chunkOn")) { m_project.chunking.enabled = value.toBool();
    } else if (key == QLatin1String("chunkSize")) {
        if (needNumber()) m_project.chunking.freqsPerTask = std::clamp(int(std::lround(d)), 1, 99);
    } else if (key == QLatin1String("signalFile")) { o.signalFile = value.toString().trimmed();
    } else if (key == QLatin1String("topology")) { o.topology = value.toBool();
    } else if (key == QLatin1String("fields")) {
        const int f = value.toInt();
        o.fields = f == 2 ? OutputSettings::Fields::AllSteps
                 : f == 1 ? OutputSettings::Fields::FirstStep : OutputSettings::Fields::Off;
    } else if (key == QLatin1String("sMatrix")) { o.sMatrix = value.toBool();
    } else if (key == QLatin1String("sMatrixFile")) { o.sMatrixFile = value.toString().trimmed();
    } else if (key == QLatin1String("sMin")) { if (needNumber()) o.sMatrixTMin = d;
    } else if (key == QLatin1String("sMax")) { if (needNumber()) o.sMatrixTMax = d;
    } else if (key == QLatin1String("pattern")) { o.directionalPattern = value.toBool();
    } else if (key == QLatin1String("patternFile")) { o.patternFile = value.toString().trimmed();
    } else if (key == QLatin1String("pMin")) { if (needNumber()) o.patternTMin = d;
    } else if (key == QLatin1String("pMax")) { if (needNumber()) o.patternTMax = d;
    }
    // Как pullFromForm оригинала: частоты из полей и окно снятия за временем счёта.
    m_project.freqs = collectFreqs();
    followTimeWindow();
    refresh();
}

QVariantMap TplDesignerModel::settings() const
{
    const Project &p = m_project;
    const OutputSettings &o = p.output;
    QVariantMap m;
    m.insert(QStringLiteral("sWide"), m_project.output.sMatrixLimitWideband);
    m.insert(QStringLiteral("name"), p.name);
    m.insert(QStringLiteral("description"), p.description);
    m.insert(QStringLiteral("kernel"), p.kernel == Kernel::X ? QStringLiteral("X") : QStringLiteral("H"));
    m.insert(QStringLiteral("xMin"), num(p.box.xMin));
    m.insert(QStringLiteral("xMax"), num(p.box.xMax));
    m.insert(QStringLiteral("yMin"), num(p.box.yMin));
    m.insert(QStringLiteral("yMax"), num(p.box.yMax));
    m.insert(QStringLiteral("boxXMin"), p.box.xMin);
    m.insert(QStringLiteral("boxXMax"), p.box.xMax);
    m.insert(QStringLiteral("boxYMin"), p.box.yMin);
    m.insert(QStringLiteral("boxYMax"), p.box.yMax);
    m.insert(QStringLiteral("delta"), num(p.delta));
    m.insert(QStringLiteral("deltaValue"), p.delta);
    m.insert(QStringLiteral("timeMax"), num(p.timeMax));
    m.insert(QStringLiteral("freqRange"), m_freqRange);
    m.insert(QStringLiteral("freqSingle"), num(m_freqSingle));
    m.insert(QStringLiteral("freqStart"), num(m_freqStart));
    m.insert(QStringLiteral("freqEnd"), num(m_freqEnd));
    m.insert(QStringLiteral("freqStep"), num(m_freqStep));
    m.insert(QStringLiteral("freqCount"), int(p.freqs.size()));
    m.insert(QStringLiteral("stepCount"), p.stepCount());
    m.insert(QStringLiteral("chunkCount"), p.chunkCount());
    m.insert(QStringLiteral("freqInfo"), freqInfo());
    m.insert(QStringLiteral("freqTooMany"), p.freqs.size() > tpl::kMaxFreqs && p.chunkCount() == 1);
    m.insert(QStringLiteral("maxStep"), formatCompact(stepForMaxFreqs()));
    m.insert(QStringLiteral("chunkOn"), p.chunking.enabled);
    m.insert(QStringLiteral("chunkSize"), QString::number(p.chunking.freqsPerTask));
    m.insert(QStringLiteral("macro"), needsMacro(p));
    m.insert(QStringLiteral("signalFile"), o.signalFile);
    m.insert(QStringLiteral("topology"), o.topology);
    m.insert(QStringLiteral("fields"), o.fields == OutputSettings::Fields::AllSteps ? 2
                                       : o.fields == OutputSettings::Fields::FirstStep ? 1 : 0);
    m.insert(QStringLiteral("sMatrix"), o.sMatrix);
    m.insert(QStringLiteral("sMatrixFile"), o.sMatrixFile);
    m.insert(QStringLiteral("sMin"), num(o.sMatrixTMin));
    m.insert(QStringLiteral("sMax"), num(o.sMatrixTMax));
    m.insert(QStringLiteral("pattern"), o.directionalPattern);
    m.insert(QStringLiteral("patternFile"), o.patternFile);
    m.insert(QStringLiteral("pMin"), num(o.patternTMin));
    m.insert(QStringLiteral("pMax"), num(o.patternTMax));

    // Подсказки страницы «Вывод» — те же, что у оригинала (refreshAll).
    QString fh;
    switch (o.fields) {
    case OutputSettings::Fields::Off:
        fh = QCoreApplication::translate("TplDesigner", "Быстро. Смотреть мгновенные поля будет нечем.");
        break;
    case OutputSettings::Fields::FirstStep:
        fh = QCoreApplication::translate("TplDesigner", "Разумный выбор для диапазона: первый шаг запишется отдельно, "
                            "остальные пойдут быстро.");
        break;
    case OutputSettings::Fields::AllSteps:
        fh = QCoreApplication::translate("TplDesigner", "Очень долго: ядро переписывает всю сетку на каждом такте счёта.");
        break;
    }
    m.insert(QStringLiteral("fieldsHint"), fh);
    m.insert(QStringLiteral("fieldsSlow"), o.fields == OutputSettings::Fields::AllSteps);

    QString pi;
    bool heavy = false;
    if (o.directionalPattern) {
        const RamEstimate ram = ramEstimate(p);
        if (ram.patternTones > 1) {
            pi = QCoreApplication::translate("TplDesigner", "Сигнал из файла несёт %1 — диаграмма копится на каждой сразу. "
                                "Памяти понадобится около %2: сетка %3, накопление ДН %4.")
                     .arg(uiPlural(ram.patternTones, QCoreApplication::translate("TplDesigner", "частоту"),
                                 QCoreApplication::translate("TplDesigner", "частоты"), QCoreApplication::translate("TplDesigner", "частот")),
                          formatBytes(ram.total()), formatBytes(ram.gridBytes),
                          formatBytes(ram.patternBytes));
        } else {
            pi = QCoreApplication::translate("TplDesigner", "Памяти понадобится около %1: сетка %2, накопление ДН %3.")
                     .arg(formatBytes(ram.total()), formatBytes(ram.gridBytes),
                          formatBytes(ram.patternBytes));
        }
        heavy = ram.total() >= kRamWarnBytes || ram.patternToneRequest > kMaxFreqs;
    }
    m.insert(QStringLiteral("patternInfo"), pi);
    m.insert(QStringLiteral("patternHeavy"), heavy);

    const RamEstimate ram = ramEstimate(p);
    m.insert(QStringLiteral("memory"), formatBytes(ram.total()));
    m.insert(QStringLiteral("nodes"), QString::number(qint64(ram.nodes)));
    return m;
}

/// Строка под полями частоты (mainwindow.cpp, refreshAll).
QString TplDesignerModel::freqInfo() const
{
    const int n = int(m_project.freqs.size());
    const int steps = m_project.stepCount();
    const int chunks = m_project.chunkCount();
    if (n > tpl::kMaxFreqs && chunks == 1) {
        return QCoreApplication::translate("TplDesigner", "Частот %1 — в одно задание больше %2 нельзя: столько не берёт ни "
                              "файл .s, ни диаграмма направленности. Возьмите шаг %3 ГГц или "
                              "разложите сетку по нескольким заданиям.")
            .arg(n).arg(tpl::kMaxFreqs).arg(formatCompact(stepForMaxFreqs()));
    }
    if (chunks > 1) {
        return QCoreApplication::translate("TplDesigner", "Выйдет %1: %2_001.tpl и далее, по %3 в каждом. У каждого "
                              "задания свой .s.")
            .arg(uiPlural(chunks, QCoreApplication::translate("TplDesigner", "задание"), QCoreApplication::translate("TplDesigner", "задания"),
                        QCoreApplication::translate("TplDesigner", "заданий")),
                 m_project.name,
                 uiPlural(m_project.chunking.freqsPerTask, QCoreApplication::translate("TplDesigner", "частоте"),
                        QCoreApplication::translate("TplDesigner", "частоты"), QCoreApplication::translate("TplDesigner", "частот")));
    }
    if (n > 1) {
        return QCoreApplication::translate("TplDesigner", "%1 точек, шагов расчёта %2. Получите ДВА файла: %3.tpl и "
                              "%3_step.h — шаг описан в них один раз, поэтому "
                              "рассинхронизировать частоты нечем.")
            .arg(n).arg(steps).arg(m_project.name);
    }
    return QCoreApplication::translate("TplDesigner", "Одна частота — один обычный файл %1.tpl без макросов.")
        .arg(m_project.name);
}

// ------------------------------------------------------------------ списки

QVariantList TplDesignerModel::shapes() const
{
    QVariantList out;
    for (const Shape &s : m_project.shapes) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), s.id);
        m.insert(QStringLiteral("name"), s.name);
        m.insert(QStringLiteral("what"), shapeWhat(s));
        m.insert(QStringLiteral("kind"), kindKey(s.kind));
        m.insert(QStringLiteral("kindText"), kindText(s));
        m.insert(QStringLiteral("material"), materialKey(s.material));
        m.insert(QStringLiteral("plasma"), s.plasma.any());
        out << m;
    }
    return out;
}

QVariantList TplDesignerModel::ports() const
{
    QVariantList out;
    for (const Port &p : m_project.ports) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), p.id);
        m.insert(QStringLiteral("name"), p.name);
        m.insert(QStringLiteral("active"), p.active);
        m.insert(QStringLiteral("what"),
                 QCoreApplication::translate("TplDesigner", "%1, %2 граница, %3…%4 мм")
                     .arg(p.active ? QCoreApplication::translate("TplDesigner", "активный") : QCoreApplication::translate("TplDesigner", "пассивный"),
                          sideText(p.side), num(p.from), num(p.to)));
        out << m;
    }
    return out;
}

Shape *TplDesignerModel::shapeById(int id)
{
    for (Shape &s : m_project.shapes) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

Port *TplDesignerModel::portById(int id)
{
    for (Port &p : m_project.ports) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

void TplDesignerModel::select(int id)
{
    m_multi.clear();
    if (id == m_selectedId)
        return;
    m_selectedId = id;
    ++m_revision;
    emit changed();
}

QVariantMap TplDesignerModel::selected() const
{
    for (const Shape &s : m_project.shapes) {
        if (s.id == m_selectedId)
            return shapeMap(s);
    }
    for (const Port &p : m_project.ports) {
        if (p.id == m_selectedId)
            return portMap(p);
    }
    return {};
}

QVariantMap TplDesignerModel::shapeMap(const Shape &s) const
{
    QVariantMap m;
    m.insert(QStringLiteral("type"), QStringLiteral("shape"));
    m.insert(QStringLiteral("id"), s.id);
    m.insert(QStringLiteral("name"), s.name);
    m.insert(QStringLiteral("kind"), kindKey(s.kind));
    m.insert(QStringLiteral("kindText"), kindText(s));
    m.insert(QStringLiteral("what"), shapeWhat(s));
    m.insert(QStringLiteral("material"), materialKey(s.material));
    m.insert(QStringLiteral("contour"), s.isContour());
    m.insert(QStringLiteral("eps"), num(s.eps));
    m.insert(QStringLiteral("epsFormulaOn"), s.epsProfile.enabled && !s.epsProfile.fromFile);
    m.insert(QStringLiteral("epsFromFile"), s.epsProfile.readsFile());
    m.insert(QStringLiteral("epsFormula"), s.epsProfile.formula);
    m.insert(QStringLiteral("xMin"), num(s.xMin));
    m.insert(QStringLiteral("xMax"), num(s.xMax));
    m.insert(QStringLiteral("yMin"), num(s.yMin));
    m.insert(QStringLiteral("yMax"), num(s.yMax));
    m.insert(QStringLiteral("cx"), num(s.center.x()));
    m.insert(QStringLiteral("cy"), num(s.center.y()));
    m.insert(QStringLiteral("rMin"), num(s.rMin));
    m.insert(QStringLiteral("rMax"), num(s.rMax));
    m.insert(QStringLiteral("fiMin"), num(s.fiMin * 180.0 / M_PI));
    m.insert(QStringLiteral("fiMax"), num(s.fiMax * 180.0 / M_PI));
    m.insert(QStringLiteral("points"), pointsText(s.points));
    m.insert(QStringLiteral("plasmaN"), s.plasma.hasN);
    m.insert(QStringLiteral("plasmaNValue"), plasmaFieldText(s.plasma.nFormula, s.plasma.n));
    m.insert(QStringLiteral("plasmaB"), s.plasma.hasB);
    m.insert(QStringLiteral("plasmaBValue"), plasmaFieldText(s.plasma.bFormula, s.plasma.b));
    m.insert(QStringLiteral("plasmaS"), s.plasma.hasS);
    m.insert(QStringLiteral("plasmaSValue"), num(s.plasma.s));
    m.insert(QStringLiteral("plasmaNFile"), s.plasma.nFile);
    m.insert(QStringLiteral("plasmaBFile"), s.plasma.bFile);
    m.insert(QStringLiteral("plasmaYFile"), s.plasma.yFile);
    m.insert(QStringLiteral("vx"), num(s.vx));
    m.insert(QStringLiteral("vy"), num(s.vy));
    m.insert(QStringLiteral("plasmaY"), s.plasma.hasY);
    m.insert(QStringLiteral("plasmaYValue"), plasmaFieldText(s.plasma.yFormula, s.plasma.y));
    QVariantList present, edgeMat;
    for (int e = 0; e < 4; ++e) {
        present << s.edges[size_t(e)].present;
        edgeMat << (s.edges[size_t(e)].custom ? materialKey(s.edges[size_t(e)].material)
                                              : QString());
    }
    m.insert(QStringLiteral("edgePresent"), present);
    m.insert(QStringLiteral("edgeMaterial"), edgeMat);
    m.insert(QStringLiteral("uniformEdges"), s.hasUniformEdges());
    return m;
}

QVariantMap TplDesignerModel::portMap(const Port &p) const
{
    QVariantMap m;
    m.insert(QStringLiteral("type"), QStringLiteral("port"));
    m.insert(QStringLiteral("id"), p.id);
    m.insert(QStringLiteral("name"), p.name);
    m.insert(QStringLiteral("side"), sideKey(p.side));
    m.insert(QStringLiteral("alongY"), portAlongY(p.side));
    m.insert(QStringLiteral("from"), num(p.from));
    m.insert(QStringLiteral("to"), num(p.to));
    m.insert(QStringLiteral("plane"), num(portPlane(p, m_project.box)));
    m.insert(QStringLiteral("active"), p.active);
    m.insert(QStringLiteral("mode"), p.mode == PortMode::Plane ? QStringLiteral("plane")
                                                              : QStringLiteral("h10"));
    m.insert(QStringLiteral("eps"), num(p.eps));
    m.insert(QStringLiteral("source"), p.signal.source == SignalSource::File
                                           ? QStringLiteral("file") : QStringLiteral("formula"));
    m.insert(QStringLiteral("shape"), p.signal.shape == SignalShape::Gaussian ? QStringLiteral("gauss")
                                      : p.signal.shape == SignalShape::Rectangular
                                          ? QStringLiteral("rect") : QStringLiteral("cont"));
    m.insert(QStringLiteral("amplitude"), num(p.signal.amplitude));
    m.insert(QStringLiteral("phase"), num(p.signal.phase * 180.0 / M_PI));
    m.insert(QStringLiteral("center"), num(p.signal.center));
    m.insert(QStringLiteral("duration"), num(p.signal.duration));
    m.insert(QStringLiteral("file"), p.signal.file);
    m.insert(QStringLiteral("sigName"), signalFileName(m_project, p));
    m.insert(QStringLiteral("ampExpression"), ampExpression(p.signal));

    const Waveshape &w = p.signal.wave;
    m.insert(QStringLiteral("waveKind"), int(w.kind));
    m.insert(QStringLiteral("waveName"), waveformName(w.kind));
    m.insert(QStringLiteral("waveSummary"), waveshapeSummary(w));
    m.insert(QStringLiteral("wAmplitude"), num(w.amplitude));
    m.insert(QStringLiteral("wStart"), num(w.start));
    m.insert(QStringLiteral("wDuration"), num(w.duration));
    m.insert(QStringLiteral("wF1"), num(w.f1));
    m.insert(QStringLiteral("wF2"), num(w.f2));
    m.insert(QStringLiteral("wTones"), QString::number(w.tones));
    m.insert(QStringLiteral("wPeriods"), QString::number(w.periods));
    m.insert(QStringLiteral("wWidth"), num(w.width));
    m.insert(QStringLiteral("wRise"), num(w.rise));
    m.insert(QStringLiteral("wFall"), num(w.fall));
    m.insert(QStringLiteral("wExpression"), w.expression);
    const WaveFields f = waveFields(w.kind);
    QVariantMap wf;
    wf.insert(QStringLiteral("band"), f.band);
    wf.insert(QStringLiteral("tones"), f.tones);
    wf.insert(QStringLiteral("carrier"), f.carrier);
    wf.insert(QStringLiteral("rate"), f.rate);
    wf.insert(QStringLiteral("periods"), f.periods);
    wf.insert(QStringLiteral("duration"), f.duration);
    wf.insert(QStringLiteral("width"), f.width);
    wf.insert(QStringLiteral("edges"), f.edges);
    wf.insert(QStringLiteral("expression"), f.expression);
    wf.insert(QStringLiteral("table"), f.table);
    m.insert(QStringLiteral("waveFields"), wf);
    m.insert(QStringLiteral("suggestedFreq"), num(suggestedFreq(w)));
    return m;
}

QVariantList TplDesignerModel::waveforms() const
{
    // Табличную форму на телефоне взять неоткуда (импорт двух колонок — функция
    // настольной версии), поэтому её в списке нет; чужой .sig с таблицей при чтении
    // готового задания сохраняется как есть.
    QVariantList out;
    for (int k = int(Waveform::MultiTone); k <= int(Waveform::Expression); ++k) {
        QVariantMap m;
        m.insert(QStringLiteral("kind"), k);
        m.insert(QStringLiteral("name"), waveformName(Waveform(k)));
        m.insert(QStringLiteral("hint"), waveformHint(Waveform(k)));
        out << m;
    }
    return out;
}

void TplDesignerModel::setShape(int id, const QString &key, const QVariant &value)
{
    Shape *s = shapeById(id);
    if (!s)
        return;
    bool ok = true;
    const double d = toDouble(value, &ok);
    if (!ok && (key == QLatin1String("eps") || key.startsWith(QLatin1String("x"))
                || key.startsWith(QLatin1String("y")) || key == QLatin1String("cx")
                || key == QLatin1String("cy") || key.startsWith(QLatin1String("r"))
                || key.startsWith(QLatin1String("fi")))) {
        setNote(QCoreApplication::translate("TplDesigner", "Не число: «%1»").arg(value.toString()));
        return;
    }
    snapshot();
    s = shapeById(id);
    if (key == QLatin1String("name")) {
        s->name = value.toString();
    } else if (key == QLatin1String("material")) {
        s->material = materialFromKey(value.toString());
    } else if (key == QLatin1String("eps")) {
        s->eps = d;
    } else if (key == QLatin1String("epsFormulaOn")) {
        s->epsProfile.enabled = value.toBool();
        s->epsProfile.fromFile = false;
        if (s->epsProfile.enabled)
            s->material = Material::Dielectric;
    } else if (key == QLatin1String("epsFormula")) {
        s->epsProfile.formula = value.toString().trimmed();
    } else if (key == QLatin1String("xMin")) { s->xMin = d;
    } else if (key == QLatin1String("xMax")) { s->xMax = d;
    } else if (key == QLatin1String("yMin")) { s->yMin = d;
    } else if (key == QLatin1String("yMax")) { s->yMax = d;
    } else if (key == QLatin1String("cx")) { s->center.setX(d);
    } else if (key == QLatin1String("cy")) { s->center.setY(d);
    } else if (key == QLatin1String("rMin")) { s->rMin = d;
    } else if (key == QLatin1String("rMax")) { s->rMax = d;
    } else if (key == QLatin1String("fiMin")) { s->fiMin = d * M_PI / 180.0;
    } else if (key == QLatin1String("fiMax")) { s->fiMax = d * M_PI / 180.0;
    } else if (key == QLatin1String("points")) {
        bool pok = false;
        const QVector<QPointF> pts = parsePoints(value.toString(), &pok);
        if (pok)
            s->points = pts;
        else
            setNote(QCoreApplication::translate("TplDesigner", "Точки — по одной на строке: «x; y»"));
    } else if (key == QLatin1String("plasmaS")) { s->plasma.hasS = value.toBool();
    } else if (key == QLatin1String("plasmaSValue")) {
        if (ok) s->plasma.s = d;
    } else if (key == QLatin1String("plasmaNFile")) { s->plasma.nFile = value.toString().trimmed();
    } else if (key == QLatin1String("plasmaBFile")) { s->plasma.bFile = value.toString().trimmed();
    } else if (key == QLatin1String("plasmaYFile")) { s->plasma.yFile = value.toString().trimmed();
    } else if (key == QLatin1String("vx")) {
        if (ok) s->vx = d;
    } else if (key == QLatin1String("vy")) {
        if (ok) s->vy = d;
    } else if (key == QLatin1String("plasmaN")) { s->plasma.hasN = value.toBool();
    } else if (key == QLatin1String("plasmaB")) { s->plasma.hasB = value.toBool();
    } else if (key == QLatin1String("plasmaY")) { s->plasma.hasY = value.toBool();
    } else if (key == QLatin1String("plasmaNValue")) {
        setPlasmaField(value.toString(), &s->plasma.n, &s->plasma.nFormula);
    } else if (key == QLatin1String("plasmaBValue")) {
        setPlasmaField(value.toString(), &s->plasma.b, &s->plasma.bFormula);
    } else if (key == QLatin1String("plasmaYValue")) {
        setPlasmaField(value.toString(), &s->plasma.y, &s->plasma.yFormula);
    } else if (key.startsWith(QLatin1String("edgePresent"))) {
        const int e = key.right(1).toInt();
        if (e >= 0 && e < 4)
            s->edges[size_t(e)].present = value.toBool();
    } else if (key.startsWith(QLatin1String("edgeMaterial"))) {
        const int e = key.right(1).toInt();
        if (e >= 0 && e < 4) {
            const QString k = value.toString();
            s->edges[size_t(e)].custom = !k.isEmpty();
            if (!k.isEmpty())
                s->edges[size_t(e)].material = materialFromKey(k);
        }
    }
    // Размеры числами НЕ переставляем местами: поля правят по одному, и «X от» больше
    // старого «X до» — обычный промежуточный шаг. Вывернутый прямоугольник ловит
    // проверка (validate: «нулевой или отрицательный размер»).
    refresh();
}

/// Частоту согласования ставим САМИ (mainwindow.cpp, onEditRequested): при сигнале из
/// файла FREQ — частота, на которой ядро согласует порт.
void TplDesignerModel::applyPortFrequency(const Port &p)
{
    if (!p.active || p.signal.source != SignalSource::File)
        return;
    const double want = suggestedFreq(p.signal.wave);
    if (want > 0.0 && m_project.freqs.size() <= 1
        && (m_project.freqs.isEmpty() || std::fabs(m_project.freqs.first() - want) > 1e-9)) {
        m_project.freqs = {want};
        m_freqRange = false;
        m_freqSingle = want;
        setNote(QCoreApplication::translate("TplDesigner", "Частота задания поставлена в %1 ГГц — на ней ядро согласует "
                               "этот порт.").arg(want, 0, 'f', 4));
    }
}

void TplDesignerModel::setPort(int id, const QString &key, const QVariant &value)
{
    Port *p = portById(id);
    if (!p)
        return;
    bool ok = true;
    const double d = toDouble(value, &ok);
    static const QStringList numericKeys = {
        QStringLiteral("from"), QStringLiteral("to"), QStringLiteral("plane"),
        QStringLiteral("eps"), QStringLiteral("amplitude"), QStringLiteral("phase"),
        QStringLiteral("center"), QStringLiteral("duration"), QStringLiteral("wAmplitude"),
        QStringLiteral("wStart"), QStringLiteral("wDuration"), QStringLiteral("wF1"),
        QStringLiteral("wF2"), QStringLiteral("wTones"), QStringLiteral("wPeriods"),
        QStringLiteral("wWidth"), QStringLiteral("wRise"), QStringLiteral("wFall")};
    if (!ok && numericKeys.contains(key)) {
        setNote(QCoreApplication::translate("TplDesigner", "Не число: «%1»").arg(value.toString()));
        return;
    }
    snapshot();
    p = portById(id);
    const Box &b = m_project.box;
    Waveshape &w = p->signal.wave;
    if (key == QLatin1String("name")) {
        p->name = value.toString();
    } else if (key == QLatin1String("side")) {
        // Сторона из списка ставит вход на эту границу коробки: направление падающей
        // волны и половина коробки тогда согласны сами.
        p->side = sideFromKey(value.toString());
        p->plane.reset();
    } else if (key == QLatin1String("plane")) {
        // Вход законно стоит и внутри области; за коробку ядро его не пустит.
        const bool alongY = portAlongY(p->side);
        const double v = std::clamp(d, alongY ? b.xMin : b.yMin, alongY ? b.xMax : b.yMax);
        p->plane = v;
        p->side = directionForPlane(alongY, v, b);
    } else if (key == QLatin1String("from")) {
        p->from = d;
    } else if (key == QLatin1String("to")) {
        p->to = d;
    } else if (key == QLatin1String("active")) {
        p->active = value.toBool();
    } else if (key == QLatin1String("mode")) {
        p->mode = value.toString() == QLatin1String("plane") ? PortMode::Plane : PortMode::H10;
    } else if (key == QLatin1String("eps")) {
        p->eps = d;
    } else if (key == QLatin1String("source")) {
        p->signal.source = value.toString() == QLatin1String("file") ? SignalSource::File
                                                                      : SignalSource::Formula;
    } else if (key == QLatin1String("shape")) {
        const QString k = value.toString();
        p->signal.shape = k == QLatin1String("gauss") ? SignalShape::Gaussian
                        : k == QLatin1String("rect") ? SignalShape::Rectangular
                                                      : SignalShape::Continuous;
    } else if (key == QLatin1String("amplitude")) { p->signal.amplitude = d;
    } else if (key == QLatin1String("phase")) { p->signal.phase = d * M_PI / 180.0;
    } else if (key == QLatin1String("center")) { p->signal.center = d;
    } else if (key == QLatin1String("duration")) { p->signal.duration = d;
    } else if (key == QLatin1String("file")) {
        p->signal.file = value.toString().trimmed();
    } else if (key == QLatin1String("waveKind")) {
        const int k = value.toInt();
        if (k >= int(Waveform::MultiTone) && k <= int(Waveform::Table))
            w.kind = Waveform(k);
    } else if (key == QLatin1String("wAmplitude")) { w.amplitude = d;
    } else if (key == QLatin1String("wStart")) { w.start = d;
    } else if (key == QLatin1String("wDuration")) { w.duration = d;
    } else if (key == QLatin1String("wF1")) { w.f1 = d;
    } else if (key == QLatin1String("wF2")) { w.f2 = d;
    } else if (key == QLatin1String("wTones")) { w.tones = int(std::lround(d));
    } else if (key == QLatin1String("wPeriods")) { w.periods = int(std::lround(d));
    } else if (key == QLatin1String("wWidth")) { w.width = d;
    } else if (key == QLatin1String("wRise")) { w.rise = d;
    } else if (key == QLatin1String("wFall")) { w.fall = d;
    } else if (key == QLatin1String("wExpression")) { w.expression = value.toString().trimmed();
    }
    if (p->from > p->to)
        std::swap(p->from, p->to);
    applyPortFrequency(*p);
    refresh();
}

// ------------------------------------------------------------------ команды

int TplDesignerModel::addShape(const QString &kind, double cx, double cy, double size)
{
    snapshot();
    const double step = m_project.delta > 0.0 ? m_project.delta : 1.0;
    // Центр и размер — к шагу сетки, как привязка холста (findSnap, шаг 3).
    const auto grid = [step](double v) { return std::round(v / step) * step; };
    const double h = std::max(step, grid(size / 2.0));
    cx = grid(cx);
    cy = grid(cy);

    QStringList names;
    for (const Shape &s : m_project.shapes)
        names << s.name;

    Shape s;
    s.id = m_project.nextFreeId();
    s.material = Material::Metal;
    if (kind == QLatin1String("circle")) {
        s.kind = ShapeKind::Circle;
        s.name = nextName(names, QCoreApplication::translate("TplDesigner", "Круг"));
        s.center = QPointF(cx, cy);
        s.rMin = 0.0;
        s.rMax = h;
        s.fiMin = 0.0;
        s.fiMax = 2 * M_PI;
    } else if (kind == QLatin1String("line")) {
        s.kind = ShapeKind::Polyline;
        s.name = nextName(names, QCoreApplication::translate("TplDesigner", "Линия"));
        s.points = {QPointF(cx - h, cy), QPointF(cx + h, cy)};
    } else if (kind == QLatin1String("contour")) {
        s.kind = ShapeKind::Polygon;
        s.name = nextName(names, QCoreApplication::translate("TplDesigner", "Контур"));
        s.points = {QPointF(cx - h, cy - h), QPointF(cx + h, cy - h), QPointF(cx, cy + h)};
    } else {
        s.kind = ShapeKind::Rectangle;
        s.name = nextName(names, QCoreApplication::translate("TplDesigner", "Прямоугольник"));
        s.xMin = cx - h;
        s.xMax = cx + h;
        s.yMin = cy - h;
        s.yMax = cy + h;
    }
    m_project.shapes << s;
    m_selectedId = s.id;
    refresh();
    return s.id;
}

int TplDesignerModel::addPort(const QString &side)
{
    snapshot();
    QStringList names;
    for (const Port &p : m_project.ports)
        names << p.name;
    const Box &b = m_project.box;
    Port p;
    p.id = m_project.nextFreeId();
    p.name = nextName(names, QCoreApplication::translate("TplDesigner", "Порт"));
    p.side = sideFromKey(side);
    const bool alongY = portAlongY(p.side);
    p.from = alongY ? b.yMin : b.xMin;
    p.to = alongY ? b.yMax : b.xMax;
    // Первый порт делаем активным: пассивные в одиночку не считают ничего
    // (geometryview.cpp, mouseReleaseEvent, Tool::Port).
    p.active = m_project.activePorts().isEmpty();
    m_project.ports << p;
    m_selectedId = p.id;
    refresh();
    return p.id;
}

void TplDesignerModel::deleteSelected()
{
    if (!m_multi.isEmpty()) {
        snapshot();
        for (int gone : std::as_const(m_multi)) {
            for (int i = 0; i < m_project.shapes.size(); ++i) {
                if (m_project.shapes.at(i).id == gone) {
                    m_project.shapes.removeAt(i);
                    break;
                }
            }
            for (Variant &v : m_project.variants)
                v.disabledShapes.removeAll(gone);
            m_project.sweep.shapeIds.removeAll(gone);
        }
        m_multi.clear();
        m_selectedId = -1;
        refresh();
        return;
    }
    const int id = m_selectedId;
    if (id < 0)
        return;
    snapshot();
    bool removed = false;
    for (int i = 0; i < m_project.shapes.size(); ++i) {
        if (m_project.shapes.at(i).id == id) {
            m_project.shapes.removeAt(i);
            removed = true;
            break;
        }
    }
    for (int i = 0; !removed && i < m_project.ports.size(); ++i) {
        if (m_project.ports.at(i).id == id) {
            m_project.ports.removeAt(i);
            removed = true;
        }
    }
    // Номер ушедшей фигуры не должен висеть в вариантах, очереди и свипе.
    for (Variant &v : m_project.variants)
        v.disabledShapes.removeAll(id);
    m_project.exciters.removeAll(id);
    m_project.sweep.shapeIds.removeAll(id);
    m_selectedId = -1;
    refresh();
}

void TplDesignerModel::duplicateSelected()
{
    if (!m_multi.isEmpty()) {
        snapshot();
        QVector<int> copies;
        const QVector<int> source = m_multi;
        for (int id : source) {
            const Shape *src = shapeById(id);
            if (!src)
                continue;
            Shape s = *src;
            s.id = m_project.nextFreeId();
            s.name = QCoreApplication::translate("TplDesigner", "%1 (копия)").arg(src->name);
            const double d = 2.0 * std::max(m_project.delta, 0.001);
            s.xMin += d; s.xMax += d;
            s.yMin += d; s.yMax += d;
            s.center += QPointF(d, d);
            for (QPointF &pt : s.points)
                pt += QPointF(d, d);
            m_project.shapes << s;
            copies << s.id;
        }
        m_multi = copies;
        refresh();
        return;
    }
    const int id = m_selectedId;
    const double off = std::max(m_project.delta, 1.0) * 5.0;
    for (const Shape &src : std::as_const(m_project.shapes)) {
        if (src.id != id)
            continue;
        snapshot();
        Shape s = src;
        s.id = m_project.nextFreeId();
        s.name = QCoreApplication::translate("TplDesigner", "%1 (копия)").arg(src.name);
        s.xMin += off; s.xMax += off;
        s.yMin -= off; s.yMax -= off;
        s.center += QPointF(off, -off);
        for (QPointF &pt : s.points)
            pt += QPointF(off, -off);
        m_project.shapes << s;
        m_selectedId = s.id;
        refresh();
        return;
    }
    for (const Port &src : std::as_const(m_project.ports)) {
        if (src.id != id)
            continue;
        snapshot();
        Port p = src;
        p.id = m_project.nextFreeId();
        p.name = QCoreApplication::translate("TplDesigner", "%1 (копия)").arg(src.name);
        p.active = false;
        m_project.ports << p;
        m_selectedId = p.id;
        refresh();
        return;
    }
}

// ------------------------------------------------------------------ пересчёт

void TplDesignerModel::refresh()
{
    updateMemory();
    const Diagnostics d = validate(m_project);
    m_issues.clear();
    const QStringList errors = collapseIssues(d.errors);
    const QStringList warnings = collapseIssues(d.warnings);
    for (const QString &e : errors) {
        QVariantMap m;
        m.insert(QStringLiteral("level"), QStringLiteral("error"));
        m.insert(QStringLiteral("text"), e);
        m_issues << m;
    }
    for (const QString &w : warnings) {
        QVariantMap m;
        m.insert(QStringLiteral("level"), QStringLiteral("warning"));
        m.insert(QStringLiteral("text"), w);
        m_issues << m;
    }
    if (errors.isEmpty() && warnings.isEmpty()) {
        QVariantMap m;
        m.insert(QStringLiteral("level"), QStringLiteral("ok"));
        m.insert(QStringLiteral("text"), QCoreApplication::translate("TplDesigner", "Замечаний нет."));
        m_issues << m;
    }
    m_errorCount = int(d.errors.size());
    m_warningCount = int(d.warnings.size());

    // Предпросмотр — как у оригинала (refreshSummary): при ошибках файла нет,
    // у макро-версии показываем ОБА файла.
    if (!d.ok()) {
        m_preview = QCoreApplication::translate("TplDesigner", "Исправьте ошибки — тогда покажу файл.");
    } else if (m_project.chunkCount() > 1) {
        const QVector<TaskChunk> chunks = emitChunks(m_project);
        QString t;
        for (int i = 0; i < chunks.size(); ++i) {
            const TaskChunk &c = chunks.at(i);
            t += QCoreApplication::translate("TplDesigner", "! ######## ЗАДАНИЕ %1 из %2: %3.tpl ########\n\n%4\n")
                     .arg(i + 1).arg(chunks.size()).arg(c.name, c.tpl);
            if (!c.stepH.isEmpty())
                t += QStringLiteral("\n! ######## %1 ########\n\n%2\n").arg(c.stepHName, c.stepH);
            if (i == 2 && chunks.size() > 3) {
                t += QCoreApplication::translate("TplDesigner", "\n! … ещё %1 заданий того же вида\n").arg(chunks.size() - 3);
                break;
            }
        }
        m_preview = t;
    } else if (needsMacro(m_project)) {
        const MacroFiles f = emitMacro(m_project);
        m_preview = QCoreApplication::translate("TplDesigner", "! ######## ФАЙЛ 1 из 2: %1.tpl ########\n\n%2\n\n"
                                   "! ######## ФАЙЛ 2 из 2: %3 ########\n"
                                   "! Должен лежать РЯДОМ с .tpl — ядро ищет его только в папке\n"
                                   "! задания.\n\n%4")
                        .arg(m_project.name, f.tpl, f.stepHName, f.stepH);
    } else {
        m_preview = emitMono(m_project);
    }
    m_summary = buildSummary(d);

    if (m_selectedId >= 0 && selected().isEmpty())
        m_selectedId = -1;
    ++m_revision;
    emit changed();
}

QString TplDesignerModel::buildSummary(const Diagnostics &d) const
{
    // Сводка «Что будет посчитано» — по образцу refreshSummary() оригинала; строка
    // запуска заменена: на телефоне ядро запускает само приложение (вкладка «Расчёт»).
    const Project &p = m_project;
    QString s;
    const auto line = [&s](const QString &t) { s += t + QLatin1Char('\n'); };

    line(QCoreApplication::translate("TplDesigner", "ЗАДАНИЕ: %1.tpl").arg(p.name));
    line(QCoreApplication::translate("TplDesigner", "Ядро: %1").arg(p.kernel == Kernel::X ? QCoreApplication::translate("TplDesigner", "X (с плазмой)")
                                                              : QStringLiteral("H")));
    if (p.sweep.active()) {
        line(QCoreApplication::translate("TplDesigner", "Вид файла: макро-версия (%1.tpl + %1_step.h) — объект поворачивается "
                            "по шагам, углов %2").arg(p.name).arg(p.sweep.angles.size()));
    } else if (p.chunkCount() > 1) {
        line(QCoreApplication::translate("TplDesigner", "Вид файла: КОНВЕЙЕР — %1, по %2 частот в каждом")
                 .arg(uiPlural(p.chunkCount(), QCoreApplication::translate("TplDesigner", "задание"), QCoreApplication::translate("TplDesigner", "задания"),
                             QCoreApplication::translate("TplDesigner", "заданий")))
                 .arg(p.chunking.freqsPerTask));
    } else if (needsMacro(p)) {
        line(QCoreApplication::translate("TplDesigner", "Вид файла: макро-версия (%1.tpl + %1_step.h) — частот больше одной. "
                            "Шаг описан ОДИН раз, строк вызова %2").arg(p.name).arg(p.stepCount()));
    } else {
        line(QCoreApplication::translate("TplDesigner", "Вид файла: обычный, один #STEP"));
    }
    line(QString());
    line(QCoreApplication::translate("TplDesigner", "ОБЛАСТЬ: X %1…%2 мм, Y %3…%4 мм")
             .arg(num(p.box.xMin), num(p.box.xMax), num(p.box.yMin), num(p.box.yMax)));
    line(QCoreApplication::translate("TplDesigner", "Шаг сетки: %1 мм   Время счёта: %2 нс").arg(num(p.delta), num(p.timeMax)));
    const RamEstimate ram = ramEstimate(p);
    line(QCoreApplication::translate("TplDesigner", "Узлов сетки: %1, памяти ядру около %2")
             .arg(qint64(ram.nodes)).arg(formatBytes(ram.total())));
    if (p.freqs.size() == 1) {
        line(QCoreApplication::translate("TplDesigner", "ЧАСТОТА: %1 ГГц").arg(num(p.freqs.first())));
    } else if (!p.freqs.isEmpty()) {
        line(QCoreApplication::translate("TplDesigner", "ЧАСТОТЫ: %1…%2 ГГц, %3 точек")
                 .arg(num(p.freqs.first()), num(p.freqs.last())).arg(p.freqs.size()));
    }
    line(QString());
    line(QCoreApplication::translate("TplDesigner", "ГЕОМЕТРИЯ (%1):").arg(p.shapes.size()));
    for (const Shape &sh : p.shapes) {
        QString what;
        if (sh.plasma.any())
            what = QCoreApplication::translate("TplDesigner", "плазма — %1").arg(plasmaFull(sh.plasma));
        else if (sh.epsProfile.readsFile())
            what = QCoreApplication::translate("TplDesigner", "распределение eps из файла %1.eps").arg(epsProfileFile(sh));
        else if (sh.hasEpsProfile())
            what = QCoreApplication::translate("TplDesigner", "eps по формуле: %1").arg(sh.epsProfile.formula);
        else if (sh.material == Material::Dielectric)
            what = QCoreApplication::translate("TplDesigner", "диэлектрик, eps = %1 (в файл уйдёт добавка %2)")
                       .arg(num(sh.eps), num(sh.eps - 1.0));
        else
            what = QCoreApplication::translate("TplDesigner", "%1 (контур)").arg(materialName(sh.material));
        line(QStringLiteral("  [%1] %2 — %3, %4").arg(sh.id).arg(sh.name, kindText(sh), what));
    }
    line(QString());
    line(QCoreApplication::translate("TplDesigner", "ПОРТЫ (%1):").arg(p.ports.size()));
    for (const Port &pt : p.ports) {
        QString sig;
        if (!pt.active) {
            sig = QCoreApplication::translate("TplDesigner", "пассивный — только принимает");
        } else if (pt.signal.source == SignalSource::File) {
            sig = QCoreApplication::translate("TplDesigner", "АКТИВНЫЙ, падающая волна из файла %1.sig: %2 — %3")
                      .arg(signalFileName(p, pt), waveformName(pt.signal.wave.kind),
                           waveshapeSummary(pt.signal.wave));
        } else {
            switch (pt.signal.shape) {
            case SignalShape::Continuous:
                sig = QCoreApplication::translate("TplDesigner", "АКТИВНЫЙ, непрерывный, амплитуда %1").arg(num(pt.signal.amplitude));
                break;
            case SignalShape::Gaussian:
                sig = QCoreApplication::translate("TplDesigner", "АКТИВНЫЙ, гауссов импульс, длительность %1 нс, центр %2 нс")
                          .arg(num(pt.signal.duration), num(pt.signal.center));
                break;
            case SignalShape::Rectangular:
                sig = QCoreApplication::translate("TplDesigner", "АКТИВНЫЙ, прямоугольный импульс %1 нс").arg(num(pt.signal.duration));
                break;
            }
        }
        line(QCoreApplication::translate("TplDesigner", "  [%1] %2 — %3 граница, апертура %4…%5 мм")
                 .arg(pt.id).arg(pt.name, sideText(pt.side), num(pt.from), num(pt.to)));
        line(QStringLiteral("       %1").arg(sig));
    }
    line(QString());
    line(QCoreApplication::translate("TplDesigner", "НА ВЫХОДЕ:"));
    line(QCoreApplication::translate("TplDesigner", "  %1.t — сигналы на портах").arg(p.output.signalFile));
    if (p.output.topology)
        line(QCoreApplication::translate("TplDesigner", "  %1.tt — топология").arg(p.output.signalFile));
    if (p.output.fields != OutputSettings::Fields::Off)
        line(QCoreApplication::translate("TplDesigner", "  %1.ex / .ey / .hz — поля").arg(p.output.signalFile));
    if (p.output.sMatrix)
        line(QCoreApplication::translate("TplDesigner", "  %1.s — матрица рассеяния").arg(p.output.sMatrixFile));
    if (p.output.directionalPattern)
        line(QCoreApplication::translate("TplDesigner", "  %1.AMP / .FAZ / .DAT — диаграмма направленности").arg(p.output.patternFile));
    {
        const QMap<QString, QByteArray> sigs = signalCompanions(p);
        if (!sigs.isEmpty()) {
            line(QString());
            line(QCoreApplication::translate("TplDesigner", "РЯДОМ С ЗАДАНИЕМ (пишет программа):"));
            for (auto it = sigs.constBegin(); it != sigs.constEnd(); ++it)
                line(QCoreApplication::translate("TplDesigner", "  %1 — падающая волна, %2 КБ").arg(it.key()).arg(it.value().size() / 1024));
        }
    }
    line(QString());
    line(QCoreApplication::translate("TplDesigner", "ЗАПУСК: вкладка «Расчёт» приложения, ядро %1. Файл .s приложение "
                        "собирает в чистой рабочей папке — старые точки к новым не подмешаются.")
             .arg(p.kernel == Kernel::X ? QStringLiteral("X") : QStringLiteral("H")));
    if (!d.warnings.isEmpty()) {
        line(QString());
        line(QCoreApplication::translate("TplDesigner", "НА ЧТО ОБРАТИТЬ ВНИМАНИЕ:"));
        for (const QString &w : d.warnings)
            line(QStringLiteral("  - %1").arg(w));
    }
    return s;
}

// ------------------------------------------------------------------ файлы

QStringList TplDesignerModel::folderFiles() const
{
    if (m_taskFolder.isEmpty())
        return {};
    if (!isContentUri(m_taskFolder))
        return QDir(m_taskFolder).entryList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    QStringList names;
    if (m_exporter)
        QMetaObject::invokeMethod(m_exporter, "fileNames", Qt::DirectConnection, qReturnArg(names),
                                  m_taskFolder);
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString TplDesignerModel::localFolder() const
{
    if (!isContentUri(m_taskFolder))
        return m_taskFolder;
    // Папка SAF: текстовые файлы задачи (задание, включаемые, сигналы, ε) — во
    // внутреннюю папку; tplreader читает их оттуда обычным образом.
    const QString stage = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                          + QStringLiteral("/tpl_designer_in");
    QDir(stage).removeRecursively();
    QDir().mkpath(stage);
    static const QStringList kExt = { QStringLiteral("tpl"), QStringLiteral("h"), QStringLiteral("sig"),
                                      QStringLiteral("eps"), QStringLiteral("txt") };
    for (const QString &name : folderFiles()) {
        if (!kExt.contains(QFileInfo(name).suffix().toLower()) || !m_exporter)
            continue;
        bool copied = false;
        QMetaObject::invokeMethod(m_exporter, "copyIn", Qt::DirectConnection, qReturnArg(copied),
                                  m_taskFolder, name, QDir(stage).filePath(name));
    }
    return stage;
}

QStringList TplDesignerModel::tplFiles() const
{
    QStringList out;
    for (const QString &name : folderFiles())
        if (name.endsWith(QLatin1String(".tpl"), Qt::CaseInsensitive))
            out << name;
    return out;
}

QStringList TplDesignerModel::existingFiles() const
{
    QStringList out;
    const QStringList present = folderFiles();
    for (const QString &name : taskFileNames(m_project, m_companions)) {
        if (present.contains(name, Qt::CaseInsensitive))
            out << name;
    }
    return out;
}

QString TplDesignerModel::openTpl(const QString &fileName)
{
    const QString folder = localFolder();
    const QString path = QDir(folder).filePath(fileName);
    const ReadResult r = readTplFile(path);
    if (!r.ok())
        return QCoreApplication::translate("TplDesigner", "Не удалось прочитать %1:\n%2").arg(fileName, r.errors.join(QLatin1Char('\n')));
    snapshot();
    m_project = r.project;
    if (m_project.output.signalFile.isEmpty())
        m_project.output.signalFile = QStringLiteral("signal");
    if (m_project.output.sMatrixFile.isEmpty())
        m_project.output.sMatrixFile = QStringLiteral("smatrix");
    if (m_project.output.patternFile.isEmpty())
        m_project.output.patternFile = QStringLiteral("pattern");
    if (m_project.freqs.isEmpty())
        m_project.freqs = {4.0};
    // Готовые распределения eps (блок FILE) ядро ищет в папке задания — берём их
    // с собой (mainwindow.cpp, loadFromFile).
    m_companions.clear();
    for (const Shape &s : std::as_const(m_project.shapes)) {
        if (!s.epsProfile.readsFile())
            continue;
        const QString name = epsProfileFile(s) + QStringLiteral(".eps");
        QFile f(QDir(folder).filePath(name));
        if (f.open(QIODevice::ReadOnly))
            m_companions.insert(name, f.readAll());
    }
    m_selectedId = -1;
    pushToForm();
    refresh();

    QString msg = QCoreApplication::translate("TplDesigner", "Прочитано из %1: фигур %2, портов %3, частот %4.")
                      .arg(fileName).arg(m_project.shapes.size()).arg(m_project.ports.size())
                      .arg(m_project.freqs.size());
    if (r.wasMacro)
        msg += QCoreApplication::translate("TplDesigner", " Файл был макро-версией — макросы раскрыты.");
    if (!r.warnings.isEmpty()) {
        msg += QCoreApplication::translate("TplDesigner", "\n\nЧасть задания модель выражает не так, как в файле. Если "
                              "сохранить поверх — это изменится:\n• ")
               + collapseIssues(r.warnings).join(QStringLiteral("\n• "));
    }
    return msg;
}

bool TplDesignerModel::writeFiles(QVariantMap *result)
{
    const auto fail = [result](const QString &msg) {
        result->insert(QStringLiteral("ok"), false);
        result->insert(QStringLiteral("message"), msg);
        return false;
    };
    if (m_taskFolder.isEmpty())
        return fail(QCoreApplication::translate("TplDesigner", "Папка задачи не выбрана."));

    WriteResult wr;
    if (isContentUri(m_taskFolder) && m_exporter) {
        // Папка Android (SAF): пишем во внутреннюю папку тем же writeTask, а в
        // выбранную папку копирует оболочка приложения (exporter.copyOut).
        const QString stage = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                              + QStringLiteral("/tpl_designer_out");
        QDir(stage).removeRecursively();
        wr = writeTask(stage, m_project, m_companions);
        if (!wr.ok)
            return fail(wr.error);
        for (const QString &name : std::as_const(wr.written)) {
            bool copied = false;
            const bool invoked = QMetaObject::invokeMethod(
                m_exporter, "copyOut", Qt::DirectConnection, qReturnArg(copied),
                QDir(stage).filePath(name), m_taskFolder, name);
            if (!invoked || !copied)
                return fail(QCoreApplication::translate("TplDesigner", "Не удалось записать %1 в папку задачи.").arg(name));
        }
    } else {
        wr = writeTask(m_taskFolder, m_project, m_companions);
        if (!wr.ok)
            return fail(wr.error);
    }

    const QString tplName = wr.written.value(0);
    const QString path = isContentUri(m_taskFolder)
                             ? m_taskFolder + QLatin1Char('/') + tplName
                             : QDir(m_taskFolder).filePath(tplName);
    QString msg = QCoreApplication::translate("TplDesigner", "Записано в папку задачи:\n  %1")
                      .arg(wr.written.join(QStringLiteral("\n  ")));
    if (wr.written.size() > 1 && needsMacro(m_project)) {
        msg += QCoreApplication::translate("TplDesigner", "\n\nФайлы лежат рядом: ядро ищет %1 только в папке задания.")
                   .arg(wr.written.value(1));
    }
    if (wr.lossy) {
        msg += QCoreApplication::translate("TplDesigner", "\n\nЧасть символов описания заменена на «?»: в кодировке "
                              "задания (CP1251) их нет.");
    }
    result->insert(QStringLiteral("ok"), true);
    result->insert(QStringLiteral("message"), msg);
    result->insert(QStringLiteral("path"), path);
    result->insert(QStringLiteral("files"), wr.written);
    emit saved(path);
    emit savedTask(m_taskFolder, tplName);
    return true;
}

QVariantMap TplDesignerModel::generate()
{
    QVariantMap result;
    const Diagnostics d = validate(m_project);
    if (!d.ok()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"),
                      QCoreApplication::translate("TplDesigner", "Ядро отвергло бы такое задание. Сначала исправьте:\n• ")
                          + collapseIssues(d.errors).join(QStringLiteral("\n• ")));
        return result;
    }
    writeFiles(&result);
    return result;
}

// ------------------------------------------------------------------ числа

QString TplDesignerModel::num(double v)
{
    if (!std::isfinite(v))
        return QStringLiteral("0");
    // Полей ввода десять знаков хватает; экспоненту оставляем только для очень
    // больших и малых (N плазмы), как QString::number 'g'.
    QString s = QString::number(v, 'g', 10);
    if (s == QLatin1String("-0"))
        s = QStringLiteral("0");
    return s;
}

QVariantMap TplDesignerModel::parseNumber(const QString &text)
{
    bool ok = false;
    const double v = toDouble(text, &ok);
    QVariantMap m;
    m.insert(QStringLiteral("ok"), ok);
    m.insert(QStringLiteral("value"), v);
    return m;
}
