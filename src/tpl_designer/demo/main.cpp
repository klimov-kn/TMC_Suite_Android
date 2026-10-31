// tpl_designer_demo — настольная проверка встроенного TPL Designer (этап 2B).
//
//   tpl_designer_demo [--folder <папка задачи>]
//   tpl_designer_demo --screenshot <папка> [--folder <папка задачи>]
//       снимки шагов в размере телефона: вертикально 1080x2340, горизонтально
//       2340x1080 (логически 360x780 / 780x360, dpr 3)
//   tpl_designer_demo --make-voln <папка>
//       собрать задание «волновод с диэлектрической вставкой» (смысл
//       src/app/examples/voln) через модель страницы и записать его в папку.

#include "designermodel.h"
#include "geometrycanvas.h"

#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>

namespace {

void loadFonts()
{
    QDirIterator it(QStringLiteral(":/fonts"), {QStringLiteral("*.ttf")}, QDir::Files);
    while (it.hasNext())
        QFontDatabase::addApplicationFont(it.next());
}

/// Задание voln через модель страницы — теми же вызовами, что делают поля и кнопки.
/// Область 160x80 мм, шаг 1 мм; стенки волновода a = 60 мм (металл, толщина 1 мм),
/// диэлектрическая вставка eps = 2 на 60…100 мм; порт 1 слева активный, порт 2 справа
/// пассивный; частоты 3…7 ГГц шагом 0.2; TIME 20 нс, окно S-матрицы 18…20 нс.
/// Поля и топология выключены — проверочный прогон.
void buildVoln(TplDesignerModel *m)
{
    m->newProject();
    m->setSetting(QStringLiteral("name"), QStringLiteral("voln"));
    m->setSetting(QStringLiteral("kernel"), QStringLiteral("H"));
    m->setSetting(QStringLiteral("xMin"), QStringLiteral("0"));
    m->setSetting(QStringLiteral("xMax"), QStringLiteral("160"));
    m->setSetting(QStringLiteral("yMin"), QStringLiteral("0"));
    m->setSetting(QStringLiteral("yMax"), QStringLiteral("80"));
    m->setSetting(QStringLiteral("delta"), QStringLiteral("1"));
    m->setSetting(QStringLiteral("timeMax"), QStringLiteral("20"));
    m->setSetting(QStringLiteral("freqRange"), true);
    m->setSetting(QStringLiteral("freqStart"), QStringLiteral("3"));
    m->setSetting(QStringLiteral("freqEnd"), QStringLiteral("7"));
    m->setSetting(QStringLiteral("freqStep"), QStringLiteral("0,2"));
    m->setSetting(QStringLiteral("signalFile"), QStringLiteral("vs"));
    m->setSetting(QStringLiteral("topology"), false);
    m->setSetting(QStringLiteral("fields"), 0);
    m->setSetting(QStringLiteral("sMatrixFile"), QStringLiteral("voln"));
    m->setSetting(QStringLiteral("sMin"), QStringLiteral("18"));
    m->setSetting(QStringLiteral("sMax"), QStringLiteral("20"));

    const auto rect = [m](const QString &name, double x0, double x1, double y0, double y1) {
        const int id = m->addShape(QStringLiteral("rect"), (x0 + x1) / 2, (y0 + y1) / 2, 10);
        m->setShape(id, QStringLiteral("name"), name);
        m->setShape(id, QStringLiteral("xMin"), x0);
        m->setShape(id, QStringLiteral("xMax"), x1);
        m->setShape(id, QStringLiteral("yMin"), y0);
        m->setShape(id, QStringLiteral("yMax"), y1);
        return id;
    };
    const int diel = rect(QStringLiteral("Вставка eps=2"), 60, 100, 10, 70);
    m->setShape(diel, QStringLiteral("material"), QStringLiteral("dielectric"));
    m->setShape(diel, QStringLiteral("eps"), QStringLiteral("2"));
    rect(QStringLiteral("Стенка нижняя"), 0, 160, 9, 10);
    rect(QStringLiteral("Стенка верхняя"), 0, 160, 70, 71);

    const int p1 = m->addPort(QStringLiteral("left"));
    m->setPort(p1, QStringLiteral("from"), 10);
    m->setPort(p1, QStringLiteral("to"), 70);
    const int p2 = m->addPort(QStringLiteral("right"));
    m->setPort(p2, QStringLiteral("from"), 10);
    m->setPort(p2, QStringLiteral("to"), 70);
    m->select(diel);
}

void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

bool shoot(QQuickWindow *w, const QString &path)
{
    const QImage img = w->grabWindow();
    const bool ok = img.save(path);
    std::fprintf(stderr, "%s %dx%d %s\n", ok ? "saved" : "FAILED", img.width(), img.height(),
                 qPrintable(QDir::toNativeSeparators(path)));
    return ok;
}

} // namespace

int main(int argc, char *argv[])
{
    // Снимки — в размере телефона: логически 360x780, dpr 3. Масштаб задаётся до
    // создания приложения, окно рисуется вне экрана (offscreen), чтобы 2340 точек
    // по высоте не упирались в экран компьютера.
    bool shots = false;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--screenshot") == 0)
            shots = true;
    }
    if (shots) {
        qputenv("QT_SCALE_FACTOR", "3");
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption folderOpt(QStringLiteral("folder"), QStringLiteral("Папка задачи"),
                                       QStringLiteral("dir"));
    const QCommandLineOption shotOpt(QStringLiteral("screenshot"), QStringLiteral("Снимки в папку"),
                                     QStringLiteral("dir"));
    const QCommandLineOption volnOpt(QStringLiteral("make-voln"), QStringLiteral("Задание voln в папку"),
                                     QStringLiteral("dir"));
    parser.addOptions({folderOpt, shotOpt, volnOpt});
    parser.process(app);

    if (parser.isSet(volnOpt)) {
        TplDesignerModel model;
        model.setTaskFolder(parser.value(volnOpt));
        buildVoln(&model);
        const QVariantMap r = model.generate();
        std::fprintf(stdout, "%s\n", qPrintable(r.value(QStringLiteral("message")).toString()));
        return r.value(QStringLiteral("ok")).toBool() ? 0 : 1;
    }

    loadFonts();
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("folder"), parser.value(folderOpt)}});
    engine.loadFromModule("TplDesignerDemo", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!win)
        return 1;
    if (!shots)
        return app.exec();

    // --- снимки ---
    QDir out(parser.value(shotOpt));
    out.mkpath(QStringLiteral("."));
    QObject *page = win->property("page").value<QObject *>();
    auto *model = page->property("designer").value<TplDesignerModel *>();
    QObject *geo = page->property("geometry").value<QObject *>();
    auto *canvas = geo->property("canvas").value<GeometryCanvas *>();
    if (!model || !canvas)
        return 1;
    buildVoln(model);

    struct Shot {
        const char *name;
        int step;
        int selectWhat;   // 0 — ничего, 1 — вставка, 2 — порт 1
        bool editor;      // свойства отдельным экраном (вертикально)
    };
    const Shot list[] = {
        {"1_task", 0, 0, false},
        {"2_geometry", 1, 1, false},
        {"3_geometry_port", 1, 2, true},
        {"4_objects", 2, 1, false},
        {"5_output", 3, 0, false},
        {"6_check", 4, 0, false},
    };
    int ok = 0, total = 0;
    for (const bool landscape : {false, true}) {
        win->resize(landscape ? QSize(780, 360) : QSize(360, 780));
        wait(150);
        for (const Shot &s : list) {
            int id = -1;
            if (s.selectWhat == 1)
                id = model->shapes().value(0).toMap().value(QStringLiteral("id")).toInt();
            else if (s.selectWhat == 2)
                id = model->ports().value(0).toMap().value(QStringLiteral("id")).toInt();
            model->select(id);
            geo->setProperty("editorOpen", s.editor);
            page->setProperty("step", s.step);
            if (s.step == 1)
                canvas->zoomToFit();
            wait(200);
            ++total;
            ok += shoot(win, out.filePath(QStringLiteral("%1_%2.png")
                                              .arg(landscape ? QStringLiteral("land")
                                                             : QStringLiteral("port"),
                                                   QString::fromLatin1(s.name))))
                      ? 1 : 0;
        }
    }
    // Поворот не теряет состояние: выделение и масштаб холста после двух поворотов.
    model->select(model->shapes().value(0).toMap().value(QStringLiteral("id")).toInt());
    page->setProperty("step", 1);
    win->resize(QSize(360, 780));
    wait(150);
    canvas->zoomBy(1.6, 180, 300);
    const double scaleBefore = canvas->pixelsPerMm();
    const int selBefore = model->selectedId();
    win->resize(QSize(780, 360));
    wait(150);
    ++total;
    ok += shoot(win, out.filePath(QStringLiteral("land_7_rotated_zoom.png"))) ? 1 : 0;
    win->resize(QSize(360, 780));
    wait(150);
    const bool kept = qFuzzyCompare(canvas->pixelsPerMm(), scaleBefore) && model->selectedId() == selBefore;
    std::fprintf(stderr, "rotation keeps state: %s (scale %.3f -> %.3f, selected %d -> %d)\n",
                 kept ? "yes" : "NO", scaleBefore, canvas->pixelsPerMm(), selBefore,
                 model->selectedId());
    ++total;
    ok += shoot(win, out.filePath(QStringLiteral("port_7_rotated_back.png"))) ? 1 : 0;
    return (ok == total && kept) ? 0 : 1;
}
