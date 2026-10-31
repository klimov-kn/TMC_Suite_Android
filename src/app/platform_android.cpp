/// @file platform_android.cpp
/// @brief Platform — Android. См. platform.h.
///
/// Java-часть: platforms/android/src/ru/tamic/app/TamicBridge.java (SAF, «Поделиться»,
/// экстры запуска) и CalcService.java (служба переднего плана, П3).
/// @see https://www.tamic.ru/docs/platforms/android.html#platform

#include "platform.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QStandardPaths>
#include <QtCore/private/qandroidextras_p.h>

#include <android/log.h>

namespace {

constexpr const char *kBridge = "ru/tamic/app/TamicBridge";
constexpr int kPickFolderRequest = 4711;

jobject context()
{
    static QJniObject ctx = QNativeInterface::QAndroidApplication::context();
    return ctx.object();
}

QJniObject jstr(const QString &s)
{
    return QJniObject::fromString(s);
}

QStringList toStringList(const QJniObject &arrayObj)
{
    QStringList out;
    if (!arrayObj.isValid())
        return out;
    QJniEnvironment env;
    auto arr = arrayObj.object<jobjectArray>();
    const jsize n = env->GetArrayLength(arr);
    out.reserve(n);
    for (jsize i = 0; i < n; ++i) {
        jobject el = env->GetObjectArrayElement(arr, i);
        out.append(QJniObject(el).toString());
        env->DeleteLocalRef(el);
    }
    return out;
}

QJniObject toJavaStringArray(const QStringList &list)
{
    QJniEnvironment env;
    jclass strClass = env.findClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(jsize(list.size()), strClass, nullptr);
    for (qsizetype i = 0; i < list.size(); ++i) {
        QJniObject s = jstr(list.at(i));
        env->SetObjectArrayElement(arr, jsize(i), s.object());
    }
    QJniObject res(arr);
    env->DeleteLocalRef(arr);
    return res;
}

// Кнопка «Остановить» уведомления службы расчёта (CalcService -> TamicBridge).
void JNICALL nativeStopRequested(JNIEnv *, jclass)
{
    QMetaObject::invokeMethod(Platform::instance(), []() {
        Platform::instance()->notifyStopRequested();
    }, Qt::QueuedConnection);
}

void registerNatives()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    const JNINativeMethod methods[] = {
        { "nativeStopRequested", "()V", reinterpret_cast<void *>(nativeStopRequested) },
    };
    QJniEnvironment env;
    if (!env.registerNativeMethods(kBridge, methods, 1))
        Platform::log(QStringLiteral("JNI: nativeStopRequested не зарегистрирован"));
}

} // namespace

Platform *Platform::instance()
{
    static Platform *p = [] {
        auto *pl = new Platform;
        registerNatives();
        return pl;
    }();
    return p;
}

QString Platform::dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString Platform::externalDir()
{
    const QString d = QJniObject::callStaticObjectMethod(
        kBridge, "externalFilesDir", "(Landroid/content/Context;)Ljava/lang/String;",
        context()).toString();
    return d.isEmpty() ? dataDir() : d;
}

QString Platform::nativeLibraryDir()
{
    static const QString dir = QJniObject::callStaticObjectMethod(
        kBridge, "nativeLibraryDir", "(Landroid/content/Context;)Ljava/lang/String;",
        context()).toString();
    return dir;
}

QString Platform::launchExtra(const QString &key)
{
    return QJniObject::callStaticObjectMethod(
        kBridge, "launchExtra", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
        context(), jstr(key).object<jstring>()).toString();
}

void Platform::prepareForTest()
{
    QJniObject::callStaticMethod<void>(
        kBridge, "prepareForTest", "(Landroid/content/Context;)V", context());
}

bool Platform::isContentUri(const QString &folder)
{
    return folder.startsWith(QLatin1String("content://"));
}

void Platform::pickFolder()
{
    const QJniObject intent = QJniObject::callStaticObjectMethod(
        kBridge, "folderPickIntent", "()Landroid/content/Intent;");
    QtAndroidPrivate::startActivity(intent, kPickFolderRequest,
        [](int, int resultCode, const QJniObject &data) {
            QString uri;
            constexpr int RESULT_OK = -1;
            if (resultCode == RESULT_OK && data.isValid()) {
                uri = QJniObject::callStaticObjectMethod(
                    kBridge, "onFolderPicked",
                    "(Landroid/content/Context;Landroid/content/Intent;)Ljava/lang/String;",
                    context(), data.object()).toString();
            }
            QMetaObject::invokeMethod(Platform::instance(), [uri]() {
                Platform::instance()->notifyFolderPicked(uri);
            }, Qt::QueuedConnection);
        });
}

QString Platform::folderTitle(const QString &uri)
{
    return QJniObject::callStaticObjectMethod(
        kBridge, "folderTitle", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
        context(), jstr(uri).object<jstring>()).toString();
}

bool Platform::hasAccess(const QString &uri)
{
    return QJniObject::callStaticMethod<jboolean>(
        kBridge, "hasAccess", "(Landroid/content/Context;Ljava/lang/String;)Z",
        context(), jstr(uri).object<jstring>());
}

void Platform::releaseAccess(const QString &uri)
{
    QJniObject::callStaticMethod<void>(
        kBridge, "releaseAccess", "(Landroid/content/Context;Ljava/lang/String;)V",
        context(), jstr(uri).object<jstring>());
}

QList<FolderEntry> Platform::listFolder(const QString &uri)
{
    const QStringList rows = toStringList(QJniObject::callStaticObjectMethod(
        kBridge, "listFolder", "(Landroid/content/Context;Ljava/lang/String;)[Ljava/lang/String;",
        context(), jstr(uri).object<jstring>()));
    QList<FolderEntry> out;
    for (const QString &row : rows) {
        const QStringList f = row.split(QChar(0x1f));
        if (f.size() < 4)
            continue;
        FolderEntry e;
        e.name = f.at(0);
        e.size = f.at(1).toLongLong();
        e.modifiedMs = f.at(2).toLongLong();
        e.isDir = f.at(3) == QLatin1String("1");
        out.append(e);
    }
    return out;
}

QByteArray Platform::readFile(const QString &uri, const QString &name, qint64 maxBytes)
{
    const QJniObject arr = QJniObject::callStaticObjectMethod(
        kBridge, "readFile", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;J)[B",
        context(), jstr(uri).object<jstring>(), jstr(name).object<jstring>(), jlong(maxBytes));
    if (!arr.isValid())
        return {};
    QJniEnvironment env;
    auto bytes = arr.object<jbyteArray>();
    const jsize n = env->GetArrayLength(bytes);
    QByteArray out(n, Qt::Uninitialized);
    env->GetByteArrayRegion(bytes, 0, n, reinterpret_cast<jbyte *>(out.data()));
    return out;
}

bool Platform::copyToLocal(const QString &uri, const QString &name, const QString &localPath)
{
    return QJniObject::callStaticMethod<jboolean>(
        kBridge, "copyToLocal",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
        context(), jstr(uri).object<jstring>(), jstr(name).object<jstring>(),
        jstr(localPath).object<jstring>());
}

bool Platform::copyFromLocal(const QString &localPath, const QString &uri, const QString &name)
{
    return QJniObject::callStaticMethod<jboolean>(
        kBridge, "copyFromLocal",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
        context(), jstr(localPath).object<jstring>(), jstr(uri).object<jstring>(),
        jstr(name).object<jstring>());
}

bool Platform::deleteFile(const QString &uri, const QString &name)
{
    return QJniObject::callStaticMethod<jboolean>(
        kBridge, "deleteFile", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z",
        context(), jstr(uri).object<jstring>(), jstr(name).object<jstring>());
}

bool Platform::share(const QString &folder, const QStringList &names)
{
    const QJniObject arr = toJavaStringArray(names);
    return QJniObject::callStaticMethod<jboolean>(
        kBridge, "share", "(Landroid/content/Context;Ljava/lang/String;[Ljava/lang/String;)Z",
        context(), jstr(folder).object<jstring>(), arr.object<jobjectArray>());
}

void Platform::requestNotificationPermission()
{
    QJniObject::callStaticMethod<void>(
        kBridge, "requestNotificationPermission", "(Landroid/content/Context;)V", context());
}

void Platform::setLanguage(const QString &code)
{
    QJniObject::callStaticMethod<void>(kBridge, "setLanguage", "(Ljava/lang/String;)V",
                                       jstr(code).object<jstring>());
}

void Platform::serviceStart(const QString &title)
{
    QJniObject::callStaticMethod<void>(
        kBridge, "serviceStart", "(Landroid/content/Context;Ljava/lang/String;)V",
        context(), jstr(title).object<jstring>());
}

void Platform::serviceUpdate(const QString &title, const QString &text, int percent)
{
    QJniObject::callStaticMethod<void>(
        kBridge, "serviceUpdate",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;I)V",
        context(), jstr(title).object<jstring>(), jstr(text).object<jstring>(), jint(percent));
}

void Platform::serviceStop()
{
    QJniObject::callStaticMethod<void>(
        kBridge, "serviceStop", "(Landroid/content/Context;)V", context());
}

void Platform::log(const QString &line)
{
    __android_log_write(ANDROID_LOG_INFO, "TAMIC", line.toUtf8().constData());
}
