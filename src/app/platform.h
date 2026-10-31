// Platform — узкое место платформенных различий («различия платформ —
// только в platforms/<платформа>/ и за #ifdef в узких местах»).
//
// Android: platform_android.cpp + platforms/android/src/ru/tamic/app/*.java
//   (выбор папки SAF, доступ к её файлам, «Поделиться», служба расчёта).
// Настольная сборка: platform_desktop.cpp (обычные папки; служба не нужна).
#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

struct FolderEntry
{
    QString name;
    qint64 size = 0;
    qint64 modifiedMs = 0;   // мс от эпохи; 0 — неизвестно
    bool isDir = false;
};

class Platform : public QObject
{
    Q_OBJECT
public:
    static Platform *instance();

    // --- Каталоги приложения ----------------------------------------------
    // Внутренняя папка данных (Android: files/; desktop: AppLocalData).
    static QString dataDir();
    // Папка, доступная с ПК (Android: внешняя папка приложения
    // /sdcard/Android/data/ru.tamic.app/files — её можно забрать «adb pull»;
    // desktop — то же, что dataDir()).
    static QString externalDir();
    // Android: каталог нативных библиотек (там лежат ядра libtmc_rt?.so).
    static QString nativeLibraryDir();

    // --- Параметры запуска --------------------------------------------------
    // Android: строковая экстра интента запуска (am start -e key value);
    // desktop: пусто (параметры — ключами командной строки, main.cpp).
    static QString launchExtra(const QString &key);
    // Проверочный запуск (самопроверка, снимки экранов): Android — окно поверх
    // экрана блокировки, экран включается и не гаснет; desktop — ничего.
    static void prepareForTest();

    // --- Папки пользователя, выбранные системным окном (SAF) ---------------
    // Идентификатор такой папки — URI вида content://… (постоянное разрешение).
    static bool isContentUri(const QString &folder);
    // Начать выбор папки; ответ — сигнал folderPicked (пустая строка — отказ).
    void pickFolder();
    static QString folderTitle(const QString &uri);
    static bool hasAccess(const QString &uri);
    static void releaseAccess(const QString &uri);
    static QList<FolderEntry> listFolder(const QString &uri);
    static QByteArray readFile(const QString &uri, const QString &name, qint64 maxBytes);
    static bool copyToLocal(const QString &uri, const QString &name, const QString &localPath);
    static bool copyFromLocal(const QString &localPath, const QString &uri, const QString &name);
    // Удалить файл папки (результат расчёта — только по просьбе пользователя). Нет
    // файла — тоже true.
    static bool deleteFile(const QString &uri, const QString &name);

    // «Поделиться» файлами: folder — локальный путь или content://…, names — имена в ней.
    static bool share(const QString &folder, const QStringList &names);

    // --- Расчёт в фоне (Android: служба переднего плана, П3) ---------------
    static void requestNotificationPermission();
    static void serviceStart(const QString &title);
    static void serviceUpdate(const QString &title, const QString &text, int percent);
    static void serviceStop();

    // Язык интерфейса ("ru"/"en") — для надписей службы расчёта (Android: уведомление).
    static void setLanguage(const QString &code);

    // Строка в системный журнал (Android: logcat, тег TAMIC; desktop: stderr).
    static void log(const QString &line);

    // Вызывается из Java (кнопка «Остановить» в уведомлении).
    void notifyStopRequested() { emit stopRequested(); }
    void notifyFolderPicked(const QString &uri) { emit folderPicked(uri); }

signals:
    void folderPicked(const QString &uri);
    void stopRequested();

private:
    Platform() = default;
};
