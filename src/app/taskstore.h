// TaskStore — папки задач и задания (решение M16).
//
// Папка задач — каталог, где лежит задание .tpl и куда пишутся результаты:
//   * «Примеры» — встроенные учебные задания (voln, horn, plasma_nby, vfield), копируются
//     во внутреннюю память при первом запуске;
//   * папки пользователя: Android — выбранные системным окном (SAF, URI content://
//     с постоянным разрешением), настольная сборка — обычные каталоги.
// Ядру нужны обычные пути и fopen, поэтому расчёт идёт во внутренней рабочей
// папке (CalcSession): задание и включаемые файлы копируются туда, результаты —
// обратно (TaskStore::copyIn / copyOut).
//
// Выбор ветки H/X (решение M22, 🟡): переключатель с запоминанием для каждой
// задачи; по умолчанию — подсказка по тексту задания (suggestMode).
#pragma once

#include "platform.h"

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QQmlEngine;
class QJSEngine;

class TaskStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QString examplesFolder READ examplesFolder CONSTANT)
    Q_PROPERTY(bool nativeFolderPicker READ nativeFolderPicker CONSTANT)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    static TaskStore *instance();
    static TaskStore *create(QQmlEngine *, QJSEngine *);

    // Встроенные примеры: файлы в ресурсах приложения (:/examples/<пример>/…).
    struct Example { QString id; QString tpl; QStringList files; };
    static QList<Example> examples();
    // Скопировать примеры в «Примеры» (только недостающие файлы).
    void installExamples();
    // Скопировать один пример в каталог dir (для самопроверки). false — ошибка.
    static bool copyExampleTo(const QString &exampleId, const QString &dir);

    QString examplesFolder() const;
    bool nativeFolderPicker() const;
    int revision() const { return m_revision; }

    // --- Для экранов -------------------------------------------------------
    QVariantList folders() const;
    Q_INVOKABLE void pickFolder();                      // Android: системное окно SAF
    Q_INVOKABLE void addLocalFolder(const QUrl &url);   // desktop: FolderDialog
    Q_INVOKABLE void forgetFolder(const QString &folder);
    Q_INVOKABLE QString folderTitle(const QString &folder) const;
    Q_INVOKABLE QVariantList tasks(const QString &folder);
    Q_INVOKABLE QString tplText(const QString &folder, const QString &name);
    Q_INVOKABLE QVariantMap modeInfo(const QString &folder, const QString &name);
    Q_INVOKABLE void setMode(const QString &folder, const QString &name, const QString &mode);
    Q_INVOKABLE QVariantMap lastRun(const QString &folder, const QString &name);
    Q_INVOKABLE QVariantList results(const QString &folder, const QString &name);
    Q_INVOKABLE bool share(const QString &folder, const QStringList &names);
    // Положить готовый локальный файл (снимок графика) в папку задачи под именем name
    // (Android: в папку SAF). true — сохранено.
    Q_INVOKABLE bool saveFile(const QString &localPath, const QString &folder, const QString &name);
    Q_INVOKABLE static QString formatSize(qint64 bytes);
    // Имена файлов папки (без подпапок) — для TPL Designer в папке SAF.
    Q_INVOKABLE QStringList fileNames(const QString &folder) const;
    // Списки на экранах перечитать (папку изменил кто-то другой, например TPL Designer).
    Q_INVOKABLE void refresh() { bump(); }

    // Удалить результаты расчёта задачи с телефона (по просьбе пользователя). Удаляются
    // только файлы из списка последнего расчёта; задание и входные файлы (inputsOf) —
    // никогда. Когда удалено всё — заодно рабочие папки расчёта и копии для просмотра.
    // Ответ: { deleted, failed, bytes }.
    Q_INVOKABLE QVariantMap deleteResults(const QString &folder, const QString &task,
                                          const QStringList &names);
    // Все посчитанные задачи с файлами результатов (для сравнения графиков):
    // [{ folder, folderTitle, task, date, files: [имена] }], новые первыми.
    Q_INVOKABLE QVariantList allRuns();
    // Обычная папка, где лежат файлы names задачи (просмотр поля читает файлы с диска):
    // локальная папка — она сама; папка SAF — рабочая папка расчёта, если файлы там
    // те же, иначе копия во внутренней папке view/. Пусто — не удалось.
    Q_INVOKABLE QString viewFolder(const QString &folder, const QString &task, const QStringList &names);
    // Служебные копии во внутренней памяти: рабочие папки расчётов (calc/) и копии
    // файлов поля для просмотра (view/). Результаты в папках задач не трогаются.
    Q_INVOKABLE qint64 cacheSize() const;
    // Путь во внутренней временной папке (export/) для готового файла перед saveFile.
    Q_INVOKABLE QString tempPath(const QString &name) const;
    Q_INVOKABLE qint64 clearCache();   // освобождено байт

    // --- Для расчёта -------------------------------------------------------
    QList<FolderEntry> list(const QString &folder) const;
    QByteArray read(const QString &folder, const QString &name, qint64 maxBytes) const;
    // Копирование между папкой задачи и обычной папкой (вызывает и TPL Designer).
    Q_INVOKABLE bool copyIn(const QString &folder, const QString &name, const QString &localPath) const;
    Q_INVOKABLE bool copyOut(const QString &localPath, const QString &folder, const QString &name) const;
    // Задание и все файлы папки, на которые оно ссылается (#include, FILE …),
    // по цепочке. Первым — само задание.
    QStringList inputsOf(const QString &folder, const QString &tpl) const;
    void recordRun(const QString &folder, const QString &name, const QString &mode,
                   int code, double seconds, const QStringList &resultFiles);
    // Ключ задачи (для настроек и имени рабочей папки).
    static QString taskKey(const QString &folder, const QString &name);
    // Рабочая папка расчёта задачи ядром mode (H/X): <данные>/calc/<задача>_<H|X>_<ключ>.
    static QString workDirFor(const QString &folder, const QString &task, const QString &mode);

    // CP1251 -> Unicode (свой перевод: в Qt для Android нет ICU).
    static QString decodeCp1251(const QByteArray &bytes);
    // Подсказка H/X по тексту задания (без комментариев «!…»).
    static QString suggestMode(const QString &text, QString *reason);

signals:
    void foldersChanged();
    void changed();
    void folderAdded(const QString &folder);
    void folderPickFailed();

private:
    TaskStore();
    void addRecent(const QString &folder);
    QString taskText(const QString &folder, const QString &tpl) const;
    void bump() { ++m_revision; emit changed(); }

    std::unique_ptr<QSettings> m_settings;
    int m_revision = 0;
};
