/// @file main.cpp
/// @brief TAMIC — точка входа приложения (общая для Android и настольной сборки).
///
/// Параметры запуска (настольная сборка — ключи командной строки; Android — экстры
/// интента: am start -n ru.tamic.app/org.qtproject.qt.android.bindings.QtActivity -e …):
///   autotest <случаи>  — самопроверка: voln_h, voln_x, horn_h, horn_x, plasma_x или all
///                        (autotest.h); на ПК приложение закрывается с кодом 0/1;
///   screen <экран>     — открыть экран: folders, tasks, task, run, results, designer, about, manual
///                        (для снимков экрана, нажимать на телефоне нельзя);
///   task <пример>      — задача для экранов tasks/task/results: voln, horn, plasma_nby;
///   chart <файл>       — (с screen results) открыть график: имя файла результатов задачи
///                        или полный путь к файлу на ПК (.s, .DAT, .t);
///   chart-opts <ключи> — вид графика через запятую: db, polar, mode=N, pick (максимум),
///                        all (все ряды), cmp / cmpsheet (сравнение) — для снимков без касаний;
///                        с screen results без chart: delete (лист удаления), filemenu=<файл>;
///                        с screen designer: sample=<образец>, step=<0..4>;
///   screenshot <файл>  — (только ПК) снимок окна и выход;
///   window-size WxH    — (только ПК) размер окна, напр. 360x780 / 780x360 (+ QT_SCALE_FACTOR=3);
///   lang ru|en         — язык только на этот запуск (снимки руководства), выбор не меняется;
///   calc <пример>[:H|X] — посчитать пример из «Примеров» (voln, horn, plasma_nby, voln_field,
///                        eps4) или задание по пути (ПК: папка добавляется в «Задачи»);
///   frameBudgetMb, frameMax — (Android) пределы записи тактов поля для съёмки видео;
///                        на ПК без screenshot приложение закрывается с кодом ядра.
/// @see https://www.tamic.ru/docs/platforms/android.html#app

#include "autotest.h"
#include "calcsession.h"
#include "kernelrunner.h"
#include "language.h"
#include "platform.h"
#include "taskstore.h"
#include "viewers.h"

#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <cstdio>

namespace {

// Шрифты проекта: Inter — интерфейс, JetBrains Mono — числа
// и текст заданий. Оба SIL OFL 1.1 (лицензии — src/app/fonts/*-OFL.txt),
// встроены в ресурсы: на телефоне их нет. По образцу appfonts::load()
// (src/gui_common/appfonts.cpp версии для macOS), но из ресурсов, а не с диска.
int loadFonts()
{
    int loaded = 0;
    QDirIterator it(QStringLiteral(":/fonts"), { QStringLiteral("*.ttf") }, QDir::Files);
    while (it.hasNext()) {
        if (QFontDatabase::addApplicationFont(it.next()) >= 0)
            ++loaded;
    }
    return loaded;
}

// Проверочный режим: «--screenshot <файл.png>» сохраняет снимок окна после
// первого кадра и закрывает приложение (сверка оформления).
bool setupScreenshot(QQmlApplicationEngine &engine, const QString &path, int delayMs)
{
    const auto roots = engine.rootObjects();
    auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
    if (!window) {
        std::fprintf(stderr, "screenshot: no window\n");
        return false;
    }
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [window, path, delayMs]() {
        QTimer::singleShot(delayMs, window, [window, path]() {
            const QImage image = window->grabWindow();
            const bool saved = !image.isNull() && image.save(path);
            std::fprintf(stderr, "screenshot: %s\n", saved ? "saved" : "failed");
            QCoreApplication::exit(saved ? 0 : 3);
        });
    }, Qt::SingleShotConnection);
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("TAMIC"));
    QGuiApplication::setApplicationVersion(QStringLiteral(TAMIC_VERSION_STRING));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
        QStringLiteral("Save a window snapshot to <file> and quit."), QStringLiteral("file"));
    const QCommandLineOption delayOption(QStringLiteral("screenshot-delay"),
        QStringLiteral("Delay before the snapshot, ms."), QStringLiteral("ms"), QStringLiteral("300"));
    const QCommandLineOption autotestOption(QStringLiteral("autotest"),
        QStringLiteral("Self-test: voln_h, voln_x, horn_h, horn_x, plasma_x or all."), QStringLiteral("cases"));
    const QCommandLineOption screenOption(QStringLiteral("screen"),
        QStringLiteral("Open screen: folders, tasks, task, run, results, designer, about, manual."),
        QStringLiteral("name"));
    const QCommandLineOption taskOption(QStringLiteral("task"),
        QStringLiteral("Example task for --screen: voln, horn, plasma_nby."), QStringLiteral("name"));
    const QCommandLineOption sizeOption(QStringLiteral("window-size"),
        QStringLiteral("Window size in logical pixels, e.g. 360x780 (portrait) or 780x360."),
        QStringLiteral("WxH"));
    const QCommandLineOption chartOption(QStringLiteral("chart"),
        QStringLiteral("With --screen results: open a chart of <file> (.s, .DAT, .t)."), QStringLiteral("file"));
    const QCommandLineOption chartOptsOption(QStringLiteral("chart-opts"),
        QStringLiteral("Chart view: db, polar, mode=N, pick, all, cmp, cmpsheet (comma-separated)."), QStringLiteral("opts"));
    const QCommandLineOption langOption(QStringLiteral("lang"),
        QStringLiteral("Interface language for this run only: ru or en."), QStringLiteral("code"));
    const QCommandLineOption calcOption(QStringLiteral("calc"),
        QStringLiteral("Calculate an example in the Examples folder: voln, horn, plasma_nby, voln_field [:H|:X]."),
        QStringLiteral("example"));
    parser.addOptions({ chartOption, chartOptsOption, langOption, calcOption });
    parser.addOptions({ screenshotOption, delayOption, autotestOption, screenOption, taskOption,
                        sizeOption });
    parser.process(app);

    QString autotestSpec = parser.value(autotestOption);
    QString screen = parser.value(screenOption);
    QString task = parser.value(taskOption);
    QString chart = parser.value(chartOption);
    QString chartOpts = parser.value(chartOptsOption);
    QString lang = parser.value(langOption);
    QString calcSpec = parser.value(calcOption);
#ifdef Q_OS_ANDROID
    lang = Platform::launchExtra(QStringLiteral("lang"));
    calcSpec = Platform::launchExtra(QStringLiteral("calc"));
    // Съёмка видео: больше тактов поля, чем в обычном расчёте (-e frameBudgetMb 600 -e frameMax 500).
    CalcSession::instance()->setFrameLimits(Platform::launchExtra(QStringLiteral("frameBudgetMb")).toInt(),
                                            Platform::launchExtra(QStringLiteral("frameMax")).toInt());
    autotestSpec = Platform::launchExtra(QStringLiteral("autotest"));
    screen = Platform::launchExtra(QStringLiteral("screen"));
    task = Platform::launchExtra(QStringLiteral("task"));
    chart = Platform::launchExtra(QStringLiteral("chart"));
    chartOpts = Platform::launchExtra(QStringLiteral("chartOpts"));
    if (!autotestSpec.isEmpty() || !screen.isEmpty() || !calcSpec.isEmpty())
        Platform::prepareForTest();
#endif

    const int fonts = loadFonts();
    QFont uiFont(QStringLiteral("Inter"));
    uiFont.setPixelSize(16);
    QGuiApplication::setFont(uiFont);

    Platform::instance();   // Android: регистрация JNI-вызовов до первого расчёта
    Language::instance()->init(lang);   // сохранённый язык или язык системы
    TaskStore::instance()->installExamples();
    // Графики (src/viewers) читают файлы задачи тем же путём, что и расчёт (SAF на Android).
    viewers::setFileReader([](const QString &folder, const QString &name) {
        if (Platform::isContentUri(folder))
            return TaskStore::instance()->read(folder, name, qint64(1) << 30);
        QFile f(QDir(folder).filePath(name));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    });
    Platform::log(QStringLiteral("start version=%1 fonts=%2 autotest=%3 screen=%4 task=%5")
                      .arg(QStringLiteral(TAMIC_VERSION_STRING)).arg(fonts)
                      .arg(autotestSpec, screen, task));

    QQmlApplicationEngine engine;
    Language::instance()->setEngine(&engine);
    // Подписи из C++ (папки, размеры файлов) — заново на новом языке.
    QObject::connect(Language::instance(), &Language::changed, TaskStore::instance(), [] {
        emit TaskStore::instance()->foldersChanged();
        TaskStore::instance()->refresh();
    });
    QVariantMap initial{
        { QStringLiteral("startChart"), chart },
        { QStringLiteral("startChartOpts"), chartOpts },
        { QStringLiteral("startScreen"), screen },
        { QStringLiteral("startTask"), task },
    };
    // Размер окна на ПК (снимки в вертикальном и горизонтальном положении; плотность
    // экрана телефона — переменной окружения QT_SCALE_FACTOR=3).
    const QStringList size = parser.value(sizeOption).split(QLatin1Char('x'));
    if (size.size() == 2 && size.at(0).toInt() > 0 && size.at(1).toInt() > 0) {
        initial.insert(QStringLiteral("width"), size.at(0).toInt());
        initial.insert(QStringLiteral("height"), size.at(1).toInt());
    }
    engine.setInitialProperties(initial);
    engine.loadFromModule("Tamic", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;   // ошибка QML уже выведена движком

    if (parser.isSet(screenshotOption)
        && !setupScreenshot(engine, parser.value(screenshotOption), parser.value(delayOption).toInt()))
        return 2;

    if (!calcSpec.isEmpty()) {
        // Расчёт примера в «Примерах» — как нажатие «Посчитать» (снимки руководства).
        // На ПК можно и задание по пути: <папка>/<задание>.tpl[:H|X] (папка добавляется в «Задачи»).
        QString spec = calcSpec, mode = QStringLiteral("H");
        if (spec.endsWith(QLatin1String(":H"), Qt::CaseInsensitive) || spec.endsWith(QLatin1String(":X"), Qt::CaseInsensitive)) {
            mode = spec.right(1).toUpper();
            spec.chop(2);
        }
        QString folder = TaskStore::instance()->examplesFolder(), tpl;
        for (const TaskStore::Example &ex : TaskStore::examples()) {
            if (ex.id == spec)
                tpl = ex.tpl;
            else if (ex.files.contains(spec + QStringLiteral(".tpl"), Qt::CaseInsensitive))
                tpl = spec + QStringLiteral(".tpl");   // второе задание примера: xr_down, xr_off
        }
        if (tpl.isEmpty() && QFileInfo(spec).isFile()) {
            const QFileInfo fi(spec);
            TaskStore::instance()->addLocalFolder(QUrl::fromLocalFile(fi.absolutePath()));
            folder = QDir::cleanPath(fi.absolutePath());
            tpl = fi.fileName();
        }
        if (tpl.isEmpty()) {
            Platform::log(QStringLiteral("calc: unknown example ") + calcSpec);
            return 4;
        }
#ifndef Q_OS_ANDROID
        if (!parser.isSet(screenshotOption))
            QObject::connect(CalcSession::instance(), &CalcSession::finished, &app,
                             [](int code) { QCoreApplication::exit(code); });
#endif
        QTimer::singleShot(300, &app, [folder, tpl, mode]() {
            CalcSession::instance()->start(folder, tpl, mode, false);
        });
    }

    if (!autotestSpec.isEmpty()) {
        const QList<Autotest::Case> cases = Autotest::parse(autotestSpec);
        if (cases.isEmpty()) {
            Platform::log(QStringLiteral("AUTOTEST unknown case: ") + autotestSpec);
#ifndef Q_OS_ANDROID
            return 4;
#endif
        } else {
#ifdef Q_OS_ANDROID
            const bool quit = false;    // на телефоне экран остаётся (снимки)
#else
            const bool quit = !parser.isSet(screenOption) && !parser.isSet(screenshotOption);
#endif
            auto *test = new Autotest(cases, quit, &app);
            QTimer::singleShot(300, test, &Autotest::start);
        }
    }

    return QGuiApplication::exec();
}
