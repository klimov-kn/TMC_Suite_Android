#pragma once

// Модель встроенного TPL Designer для QML (этап 2B, решение M17).
//
// Держит одно задание tpl::Project — ту же модель, что и настольный TPL Designer, —
// и отдаёт его страницам в виде простых карт (QVariantMap). Синтаксиса .tpl здесь
// НЕТ: текст файла порождает только ядро генератора (emitMono / emitMacro /
// writeTask из tamic_tpl_core). Правила окна настольной версии, которые влияют на
// задание (сетка частот, окно снятия за временем счёта, частота согласования при
// сигнале из файла), перенесены сюда дословно — ссылки на mainwindow.cpp в комментариях.

#include "emitter.h"
#include "project.h"
#include "sample.h"
#include "usersamples.h"

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class TplDesignerModel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DesignerModel)

    /// Папка задачи (M16). Сюда «Сгенерировать» кладёт .tpl и сопутствующие файлы.
    Q_PROPERTY(QString taskFolder READ taskFolder WRITE setTaskFolder NOTIFY taskFolderChanged)
    /// Необязательный объект записи в папку, которую нельзя открыть QFile (Android SAF,
    /// content://): у него вызывается copyOut(localPath, folder, name) -> bool.
    Q_PROPERTY(QObject *exporter READ exporter WRITE setExporter NOTIFY exporterChanged)

    /// Растёт на каждой правке — холсту и спискам повод перерисоваться.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    /// Поля страниц «Задание» и «Вывод» (ключи — в designermodel.cpp, settings()).
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    Q_PROPERTY(QVariantList shapes READ shapes NOTIFY changed)
    Q_PROPERTY(QVariantList ports READ ports NOTIFY changed)
    /// Выделенная фигура или порт со всеми полями; пусто — ничего не выделено.
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(int selectedId READ selectedId WRITE select NOTIFY changed)
    /// Замечания проверки: [{level: "error"|"warning"|"ok", text}].
    Q_PROPERTY(QVariantList issues READ issues NOTIFY changed)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY changed)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY changed)
    Q_PROPERTY(bool canGenerate READ canGenerate NOTIFY changed)
    /// Текст файла(ов) задания, как его запишет ядро генератора.
    Q_PROPERTY(QString preview READ preview NOTIFY changed)
    /// «Что будет посчитано» — сводка, как на странице «Проверка» оригинала.
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    /// Последняя подсказка (что программа сделала сама). Пусто — нечего сказать.
    Q_PROPERTY(QString note READ note NOTIFY noteChanged)
    /// Формы падающей волны из файла: [{kind, name, hint}].
    Q_PROPERTY(QVariantList waveforms READ waveforms CONSTANT)
    /// Страница «Что считать»: входы, варианты геометрии, поворот объекта (designerextras.cpp).
    Q_PROPERTY(QVariantMap runPlan READ runPlan NOTIFY changed)
    /// Память телефона и оценка расчёта: {known, total, used, grid, pattern, fits, text, level ...}.
    Q_PROPERTY(QVariantMap memory READ memory NOTIFY memoryChanged)
    /// Номера выбранных фигур; больше одного — группа.
    Q_PROPERTY(QVariantList selectedIds READ selectedIds NOTIFY changed)
    /// Карточка образца: параметры, проверка, паспорт, числа ядра.
    Q_PROPERTY(QVariantMap sampleCard READ sampleCard NOTIFY sampleCardChanged)

public:
    explicit TplDesignerModel(QObject *parent = nullptr);

    // --- доступ из C++ (холст) ---
    const tpl::Project &project() const { return m_project; }
    /// Снимок для отмены перед правкой, которую делает холст.
    void beginEdit();
    /// Правка холста закончена: пересчитать всё и сообщить.
    void endEdit();
    /// Изменяемое задание — только между beginEdit() и endEdit().
    tpl::Project &mutableProject() { return m_project; }
    /// Предпросмотр правки во время протяжки (без снимка отмены).
    void touch();

    QString taskFolder() const { return m_taskFolder; }
    void setTaskFolder(const QString &folder);
    QObject *exporter() const { return m_exporter; }
    void setExporter(QObject *o);

    int revision() const { return m_revision; }
    QVariantMap settings() const;
    QVariantList shapes() const;
    QVariantList ports() const;
    QVariantMap selected() const;
    int selectedId() const { return m_selectedId; }
    QVariantList issues() const { return m_issues; }
    int errorCount() const { return m_errorCount; }
    int warningCount() const { return m_warningCount; }
    bool canGenerate() const { return m_errorCount == 0; }
    QString preview() const { return m_preview; }
    QString summary() const { return m_summary; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString note() const { return m_note; }
    QVariantList waveforms() const;
    QVariantMap runPlan() const;
    QVariantMap memory() const { return m_memory; }
    QVariantList selectedIds() const;
    /// Фигура выбрана (одна или в группе).
    bool isSelected(int id) const;
    /// Выбранная группа; пусто, если выбрано не больше одной фигуры.
    const QVector<int> &multi() const { return m_multi; }
    QVariantMap sampleCard() const { return m_card; }
    /// Задание карточки образца для картинки (SamplePreview).
    const tpl::Project &cardProject() const { return m_cardBuild.project; }
    /// Собрать образец по ключу; values == nullptr — по умолчанию.
    tpl::samples::Build buildSample(const QString &key, const tpl::samples::Values *values) const;
    /// Подставить задание только для показа, без проверки (SamplePreview).
    void setViewProject(const tpl::Project &p);

    // --- команды страниц ---
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE void select(int id);
    Q_INVOKABLE void setShape(int id, const QString &key, const QVariant &value);
    Q_INVOKABLE void setPort(int id, const QString &key, const QVariant &value);

    /// Новая фигура числами: kind = rect | circle | line | contour; центр и размер, мм.
    Q_INVOKABLE int addShape(const QString &kind, double cx, double cy, double size);
    /// Новый порт на стороне коробки: left | right | bottom | top.
    Q_INVOKABLE int addPort(const QString &side);
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void newProject();

    /// Задания .tpl в папке задачи — чтобы открыть готовое на правку.
    Q_INVOKABLE QStringList tplFiles() const;
    /// Прочитать готовый .tpl (из папки задачи). Ответ — текст для человека: что
    /// прочитано и что модель выражает не так, как в файле.
    Q_INVOKABLE QString openTpl(const QString &fileName);

    /// Образцы библиотеки: [{id, title, summary, items: [{key, title, summary, standard, kernel}]}].
    Q_INVOKABLE QVariantList sampleSections();
    Q_INVOKABLE void openSampleCard(const QString &key);
    Q_INVOKABLE void setSampleParam(const QString &key, const QVariant &value);
    Q_INVOKABLE void resetSampleParams();
    /// Открыть образец карточки в редакторе. Ответ — текст для человека.
    Q_INVOKABLE QString openCardInEditor();
    /// Положить текущее задание в «Мои образцы». Ответ: {ok, message}.
    Q_INVOKABLE QVariantMap saveAsSample(const QString &title, const QString &summary);
    Q_INVOKABLE bool deleteUserSample();

    // --- «Что считать» ---
    Q_INVOKABLE void setExciter(int portId, bool on);
    Q_INVOKABLE void addVariant();
    Q_INVOKABLE void presetVariants();
    Q_INVOKABLE void removeVariant(int index);
    Q_INVOKABLE void setVariant(int index, const QString &key, const QString &value);
    Q_INVOKABLE void setVariantShape(int index, int shapeId, bool on);
    Q_INVOKABLE void setSweep(const QString &key, const QVariant &value);
    Q_INVOKABLE void setSweepShape(int shapeId, bool on);

    // --- справочный калькулятор и импорт ---
    /// Отсечки волновода, длина волны, частоты плазмы (окно «Калькулятор» настольной версии).
    Q_INVOKABLE QVariantMap calculate(const QVariantMap &in) const;
    /// Модели .stl и .obj в папке задачи.
    Q_INVOKABLE QStringList meshFiles() const;
    /// Срез модели плоскостью -> обычные фигуры. plane: xy | yz | xz. Ответ — текст для человека.
    Q_INVOKABLE QString importMesh(const QString &fileName, const QString &plane, const QString &offset);

    // --- память и группа фигур ---
    /// Перечитать память телефона: страница «Проверка» вызывает это раз в несколько секунд.
    Q_INVOKABLE void updateMemory();
    /// Добавить фигуру в группу или убрать из неё.
    Q_INVOKABLE void toggleSelected(int id);
    /// Соединить выбранные линии в одну фигуру. Ответ — текст для человека.
    Q_INVOKABLE QString mergeSelected();
    /// Скруглить углы выбранного прямоугольника. Ответ — текст ошибки или пусто.
    Q_INVOKABLE QString filletSelected(const QString &radiusText);

    /// Какие из файлов задания уже лежат в папке задачи (вопрос «перезаписать?»).
    Q_INVOKABLE QStringList existingFiles() const;

    /// Записать задание в папку задачи. Ответ: {ok, message, path, files}.
    Q_INVOKABLE QVariantMap generate();

    /// Число для поля ввода: без хвостовых нулей и без экспоненты, где можно.
    Q_INVOKABLE static QString num(double v);
    /// Разбор числа из поля: принимает и запятую. ok=false — не число.
    Q_INVOKABLE static QVariantMap parseNumber(const QString &text);

signals:
    void changed();
    void taskFolderChanged();
    void exporterChanged();
    void noteChanged();
    void memoryChanged();
    void sampleCardChanged();
    /// Задание записано; путь к .tpl внутри папки задачи.
    void saved(const QString &tplPath);
    /// Задание записано: папка и имя .tpl (для перехода к нему в оболочке).
    void savedTask(const QString &folder, const QString &tplName);

private:
    void snapshot();
    void refresh();
    void setNote(const QString &n);
    void pushToForm();
    void rebuildCard();
    QVector<double> collectFreqs() const;
    double stepForMaxFreqs() const;
    void followTimeWindow();
    void applyPortFrequency(const tpl::Port &p);
    tpl::Shape *shapeById(int id);
    tpl::Port *portById(int id);
    QVariantMap shapeMap(const tpl::Shape &s) const;
    QVariantMap portMap(const tpl::Port &p) const;
    QString buildSummary(const tpl::Diagnostics &d) const;
    QString freqInfo() const;
    bool writeFiles(QVariantMap *result);
    // Папка content:// (Android SAF) недоступна через QDir: список файлов и чтение —
    // через оболочку (exporter.fileNames / exporter.copyIn).
    QStringList folderFiles() const;
    QString localFolder() const;

    tpl::Project m_project;
    QMap<QString, QByteArray> m_companions; ///< файлы рядом с .tpl (готовые .eps образцов)

    // Поля частоты — как в окне оригинала: одна частота или диапазон «от, до, шаг».
    bool m_freqRange = false;
    double m_freqSingle = 4.0;
    double m_freqStart = 3.0;
    double m_freqEnd = 5.0;
    double m_freqStep = 0.1;

    QString m_taskFolder;
    QObject *m_exporter = nullptr;
    int m_revision = 0;
    int m_selectedId = -1;
    QVariantList m_issues;
    int m_errorCount = 0;
    int m_warningCount = 0;
    QString m_preview;
    QString m_summary;
    QString m_note;
    bool m_editing = false;

    QVector<int> m_multi;                   ///< выбранная группа фигур (2 и больше)
    QVariantMap m_memory;
    QString m_cardKey;                      ///< открытая карточка образца
    tpl::samples::Values m_cardValues;
    tpl::samples::Build m_cardBuild;
    QVariantMap m_card;
    int m_cardRevision = 0;
    QVector<tpl::samples::UserSample> m_userSamples;

    QVector<tpl::Project> m_undo;
    QVector<tpl::Project> m_redo;
};
