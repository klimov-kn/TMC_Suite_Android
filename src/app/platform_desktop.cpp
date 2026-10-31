// Platform — настольная сборка (Windows, отладка). См. platform.h.
// Папки — обычные пути, выбор — FolderDialog в QML; службы нет.

#include "platform.h"

#include <QDesktopServices>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

#include <cstdio>

Platform *Platform::instance()
{
    static Platform *p = new Platform;
    return p;
}

QString Platform::dataDir()
{
    // TAMIC_DATA_DIR — своя папка данных (снимки руководства, проверки): чистое состояние.
    const QString own = qEnvironmentVariable("TAMIC_DATA_DIR");
    if (!own.isEmpty())
        return own;
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString Platform::externalDir() { return dataDir(); }
QString Platform::nativeLibraryDir() { return QString(); }
QString Platform::launchExtra(const QString &) { return QString(); }
void Platform::prepareForTest() {}

bool Platform::isContentUri(const QString &folder)
{
    return folder.startsWith(QLatin1String("content://"));
}

void Platform::pickFolder() { emit folderPicked(QString()); }
QString Platform::folderTitle(const QString &uri) { return uri; }
bool Platform::hasAccess(const QString &) { return false; }
void Platform::releaseAccess(const QString &) {}
QList<FolderEntry> Platform::listFolder(const QString &) { return {}; }
QByteArray Platform::readFile(const QString &, const QString &, qint64) { return {}; }
bool Platform::copyToLocal(const QString &, const QString &, const QString &) { return false; }
bool Platform::copyFromLocal(const QString &, const QString &, const QString &) { return false; }
bool Platform::deleteFile(const QString &, const QString &) { return false; }

bool Platform::share(const QString &folder, const QStringList &)
{
    // На ПК «Поделиться» — открыть папку с результатами в проводнике.
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void Platform::setLanguage(const QString &) {}
void Platform::requestNotificationPermission() {}
void Platform::serviceStart(const QString &) {}
void Platform::serviceUpdate(const QString &, const QString &, int) {}
void Platform::serviceStop() {}

void Platform::log(const QString &line)
{
    std::fprintf(stderr, "TAMIC: %s\n", line.toUtf8().constData());
    std::fflush(stderr);
}
