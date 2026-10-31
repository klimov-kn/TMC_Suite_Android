// Общая часть трёх графиков (S-матрица, ДН, сигналы): QQuickPaintedItem + QPainter (M5).
//
// Что умеет:
//   * ряды данных (выбор видимых — setSeriesVisible), цвета рядов по порядку из
//     style-guide (accent, accent-2, accent-3, ok, danger, text-dim);
//   * оси с «круглыми» делениями: шаг подбирается по ширине подписей, подписи не налезают;
//   * декартовы и полярные координаты (полярные — для ДН);
//   * масштаб и сдвиг (zoomAt / panBy — из жестов QML), сброс (resetView);
//   * касание точки (pick): метка со значением шрифтом JetBrains Mono;
//   * снимок в PNG (saveImage) — тем же кодом рисования, с легендой и подписью.
// Окно просмотра хранится в единицах данных, поэтому поворот экрана (новый размер
// элемента) не теряет ни масштаба, ни выбора рядов, ни отмеченной точки.
//
// Чтение файлов идёт в рабочем потоке (viewers::fileReader), обработка для выбранного
// вида оси (rebuild) — в главном.
#pragma once

#include "touchgesture.h"

#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <memory>
#include <vector>

class QPainter;

namespace viewers {

class ChartBase : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ANONYMOUS

    // Источник: папка задачи (путь или content://) и файлы. file — открытый файл,
    // files — все файлы этого вида из того же расчёта (их ряды тоже доступны в списке;
    // пусто — только file).
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY sourceChanged)
    Q_PROPERTY(QString file READ file WRITE setFile NOTIFY sourceChanged)
    Q_PROPERTY(QStringList files READ files WRITE setFiles NOTIFY sourceChanged)
    // Файлы для сравнения из других расчётов: [{ folder, file, label }] — label (например,
    // «horn2.tpl · horn.DAT») подписывает их ряды. Их ряды видны сразу, пунктиром.
    Q_PROPERTY(QVariantList compare READ compare WRITE setCompare NOTIFY sourceChanged)

    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString info READ info NOTIFY dataChanged)          // сводка (КНД, число точек …)
    Q_PROPERTY(QString kindTitle READ kindTitle NOTIFY dataChanged) // «S-матрица», «ДН», «Сигналы»

    Q_PROPERTY(QStringList modes READ modes NOTIFY dataChanged)    // виды оси Y
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool polarAvailable READ polarAvailable CONSTANT)
    Q_PROPERTY(bool polar READ polar WRITE setPolar NOTIFY modeChanged)

    // Ряды: [{ name, group, color, visible }].
    Q_PROPERTY(QVariantList series READ seriesModel NOTIFY seriesChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY seriesChanged)
    Q_PROPERTY(bool zoomed READ zoomed NOTIFY viewChanged)
    Q_PROPERTY(QString pickText READ pickText NOTIFY viewChanged)  // текст отмеченной точки

public:
    // Данные, прочитанные в рабочем потоке (своё у каждого вида графика).
    struct Doc
    {
        virtual ~Doc() = default;
    };
    struct InputFile
    {
        QString name;             // подпись группы рядов (файл или «задача · файл»)
        QByteArray bytes;
        bool compared = false;    // файл другого расчёта (сравнение)
    };
    using Loader = std::function<std::shared_ptr<Doc>(const std::vector<InputFile> &, QString *error)>;
    struct Series
    {
        QString key;              // постоянный ключ (файл + элемент) — для выбора
        QString name;             // подпись в списке
        QString group;            // группа (файл)
        int colorIndex = 0;
        bool visible = false;
        bool defaultVisible = false;
        bool compared = false;    // ряд файла сравнения — рисуется пунктиром
        std::vector<double> x, y; // точки в единицах осей (декартовы) или
                                  // x — угол (рад), y — радиус (полярные)
    };

    explicit ChartBase(QQuickItem *parent = nullptr);
    ~ChartBase() override;

    QString folder() const { return m_folder; }
    void setFolder(const QString &v);
    QString file() const { return m_file; }
    void setFile(const QString &v);
    QStringList files() const { return m_files; }
    void setFiles(const QStringList &v);
    QVariantList compare() const { return m_compare; }
    void setCompare(const QVariantList &v);

    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    QString info() const { return m_info; }
    virtual QString kindTitle() const = 0;

    QStringList modes() const { return m_modes; }
    int mode() const { return m_mode; }
    void setMode(int m);
    virtual bool polarAvailable() const { return false; }
    bool polar() const { return m_polar; }
    void setPolar(bool p);

    QVariantList seriesModel() const;
    int visibleCount() const;
    bool zoomed() const { return m_zoomed; }
    QString pickText() const;

    Q_INVOKABLE void setSeriesVisible(int index, bool visible);
    Q_INVOKABLE void zoomAt(qreal factor, qreal x, qreal y);   // factor > 1 — крупнее
    Q_INVOKABLE void panBy(qreal dx, qreal dy);                // в пикселях элемента
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void pick(qreal x, qreal y);                   // ближайшая точка рядов
    Q_INVOKABLE void pickMax();                                // максимум первого видимого ряда
    Q_INVOKABLE void clearPick();
    Q_INVOKABLE void reload();
    // Снимок в PNG во временную папку приложения; возвращает путь (пусто — ошибка).
    // Размер — как у элемента на экране, плотность — devicePixelRatio окна (не меньше 2).
    Q_INVOKABLE QString saveImage(const QString &fileName);

    // Для проверок без экрана: нарисовать в картинку заданного размера.
    QImage renderImage(const QSizeF &size, qreal dpr, bool forExport) const;

    void paint(QPainter *painter) override;

signals:
    void sourceChanged();
    void stateChanged();
    void dataChanged();
    void modeChanged();
    void seriesChanged();
    void viewChanged();

protected:
    void touchEvent(QTouchEvent *e) override;
    void touchUngrabEvent() override;

protected:
    // Функция чтения (без обращения к объекту: выполняется в рабочем потоке).
    virtual Loader loader() const = 0;
    // Подписи видов оси (setModes) на текущем языке; вызывается и при смене языка.
    virtual void retranslate() {}

    // Построить ряды для текущего вида (m_mode, m_polar) из m_doc. Вызывается после
    // загрузки и при смене вида; видимость рядов переносится по ключу.
    virtual void rebuild() = 0;
    // Нижняя граница авто-масштаба Y (дБ: не ниже максимума − 60) — по виду.
    virtual double yAutoFloorSpan() const { return 0.0; }
    // Полярные координаты: радиус в единицах y (0 — центр), подписи колец.
    virtual double polarRadius(double y) const { return y; }
    virtual QString polarRingLabel(double r) const;
    virtual std::vector<double> polarRings() const { return { 0.25, 0.5, 0.75, 1.0 }; }
    // Строки метки отмеченной точки (под именем ряда).
    virtual QStringList pointText(const Series &s, size_t i) const;

    void setModes(const QStringList &modes);
    void setInfo(const QString &info);
    void setAxisTitles(const QString &x, const QString &y) { m_xTitle = x; m_yTitle = y; }

    std::shared_ptr<Doc> m_doc;
    std::vector<Series> m_series;
    int m_mode = 0;
    bool m_polar = false;

    static QString fmt(double v, int significant = 6);

private:
    void scheduleLoad();
    void startLoad();
    void applyRebuild(bool keepX);
    QRectF autoView() const;
    QRectF plotRect(const QSizeF &size, bool forExport) const;
    QRectF effectiveView(const QRectF &plot) const;
    QPointF toScreen(const QRectF &plot, const QRectF &view, double x, double y) const;
    QPointF toData(const QRectF &plot, const QRectF &view, QPointF p) const;
    QPointF pointOf(const Series &s, size_t i) const;    // точка в плоскости графика
    void draw(QPainter *p, const QSizeF &size, bool forExport) const;
    void drawCartesianGrid(QPainter *p, const QRectF &plot, const QRectF &view) const;
    void drawPolarGrid(QPainter *p, const QRectF &plot, const QRectF &view) const;
    void drawSeries(QPainter *p, const QRectF &plot, const QRectF &view) const;
    void drawPick(QPainter *p, const QRectF &plot, const QRectF &view) const;
    void drawLegend(QPainter *p, const QRectF &area) const;

    TouchGesture m_touch;          // касания (щипок без скачка)
    QString m_folder, m_file;
    QStringList m_files;
    QVariantList m_compare;
    bool m_loading = false;
    bool m_loadQueued = false;
    QString m_error;
    QString m_info;
    QStringList m_modes;
    QString m_xTitle, m_yTitle;
    QSet<QString> m_selected;      // ключи видимых рядов (переживают перезагрузку и смену вида)
    QSet<QString> m_seen;          // ключи рядов, уже бывших в списке (новые — по умолчанию)
    bool m_selectionTouched = false;
    QRectF m_view;                 // окно просмотра (единицы плоскости графика)
    bool m_zoomed = false;
    int m_pickSeries = -1;
    qint64 m_pickIndex = -1;
    quint64 m_generation = 0;
};

} // namespace viewers
