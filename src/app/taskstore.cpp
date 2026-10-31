/// @file taskstore.cpp
/// @brief TaskStore — папки задач и задания. См. taskstore.h.
/// @see https://www.tamic.ru/docs/platforms/android.html#files

#include "taskstore.h"

#include "viewers.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJSEngine>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

constexpr qint64 kMaxTextBytes = 4 * 1024 * 1024;   // задание и включаемые файлы
constexpr const char *kExamplesVersion = "4";   // 2 — vfield, 3 — eps4, xr_*, 4 — линзы lens1, lens3

QString localPath(const QString &folder, const QString &name)
{
    return QDir(folder).filePath(name);
}

bool isTpl(const QString &name)
{
    return name.endsWith(QLatin1String(".tpl"), Qt::CaseInsensitive);
}

// Текст без комментариев: в задании «!» начинает комментарий до конца строки
// (препроцессор prepr). В комментариях встречаются имена выходных файлов
// («horn.t -> TMCROS»), их не считаем входными.
QString stripComments(const QString &text)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &l : lines) {
        const qsizetype k = l.indexOf(QLatin1Char('!'));
        if (k >= 0)
            l.truncate(k);
    }
    return lines.join(QLatin1Char('\n'));
}

// Имя файла встречается в тексте отдельным словом (без учёта регистра:
// Windows-задания пишут имена как попало, И-1).
bool mentions(const QString &text, const QString &name)
{
    const QString pattern = QStringLiteral("(?<![A-Za-z0-9_.\\\\/])")
        + QRegularExpression::escape(name) + QStringLiteral("(?![A-Za-z0-9_.])");
    const QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
    return re.match(text).hasMatch();
}

} // namespace

// ------------------------------------------------------------------ создание

TaskStore *TaskStore::instance()
{
    static TaskStore *s = new TaskStore;
    return s;
}

TaskStore *TaskStore::create(QQmlEngine *, QJSEngine *)
{
    TaskStore *s = instance();
    QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

TaskStore::TaskStore()
{
    QDir().mkpath(Platform::dataDir());
    m_settings = std::make_unique<QSettings>(
        QDir(Platform::dataDir()).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    connect(Platform::instance(), &Platform::folderPicked, this, [this](const QString &uri) {
        if (uri.isEmpty()) {
            emit folderPickFailed();
            return;
        }
        addRecent(uri);
        emit folderAdded(uri);
    });
}

// ------------------------------------------------------------------ примеры

QList<TaskStore::Example> TaskStore::examples()
{
    return {
        { QStringLiteral("voln"), QStringLiteral("VOLN.TPL"),
          { QStringLiteral("VOLN.TPL"), QStringLiteral("VOLNSTEP.H") } },
        { QStringLiteral("horn"), QStringLiteral("horn.tpl"), { QStringLiteral("horn.tpl") } },
        { QStringLiteral("plasma_nby"), QStringLiteral("plasma_nby.tpl"),
          { QStringLiteral("plasma_nby.tpl") } },
        // Волновод VOLN на одной частоте с записью поля (FIELDS, TOPOLOGY) — для экрана «Поле».
        { QStringLiteral("voln_field"), QStringLiteral("VFIELD.TPL"),
          { QStringLiteral("VFIELD.TPL"), QStringLiteral("VFIELD.H") } },
        // Наглядная проверка поля: вставка eps = 4 замедляет волну;
        // X-образная решётка плазменных трубок поворачивает волну вверх, вниз или пропускает.
        { QStringLiteral("eps4"), QStringLiteral("eps4.tpl"), { QStringLiteral("eps4.tpl") } },
        { QStringLiteral("xr"), QStringLiteral("xr_up.tpl"),
          { QStringLiteral("xr_up.tpl"), QStringLiteral("xr_down.tpl"), QStringLiteral("xr_off.tpl") } },
        // Линза Люнеберга: один облучатель — плоская волна; три — три луча.
        { QStringLiteral("lens"), QStringLiteral("lens1.tpl"),
          { QStringLiteral("lens1.tpl"), QStringLiteral("lens3.tpl") } },
    };
}

QString TaskStore::examplesFolder() const
{
    return QDir(Platform::dataDir()).filePath(QStringLiteral("examples"));
}

bool TaskStore::nativeFolderPicker() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

static bool copyResource(const QString &res, const QString &dst)
{
    QFile::remove(dst);
    if (!QFile::copy(res, dst))
        return false;
    // Файлы из ресурсов копируются «только для чтения» — снять запрет.
    QFile::setPermissions(dst, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup
                                   | QFile::ReadOther);
    return true;
}

void TaskStore::installExamples()
{
    const QString dir = examplesFolder();
    QDir().mkpath(dir);
    const bool fresh = m_settings->value(QStringLiteral("examples/version")).toString()
                       != QLatin1String(kExamplesVersion);
    for (const Example &ex : examples()) {
        for (const QString &f : ex.files) {
            const QString dst = localPath(dir, f);
            if (fresh || !QFileInfo::exists(dst))
                copyResource(QStringLiteral(":/examples/%1/%2").arg(ex.id, f), dst);
        }
    }
    m_settings->setValue(QStringLiteral("examples/version"), QLatin1String(kExamplesVersion));
}

bool TaskStore::copyExampleTo(const QString &exampleId, const QString &dir)
{
    for (const Example &ex : examples()) {
        if (ex.id != exampleId)
            continue;
        QDir().mkpath(dir);
        for (const QString &f : ex.files) {
            if (!copyResource(QStringLiteral(":/examples/%1/%2").arg(ex.id, f), localPath(dir, f)))
                return false;
        }
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ папки

QVariantList TaskStore::folders() const
{
    QVariantList out;
    QVariantMap ex;
    ex.insert(QStringLiteral("id"), examplesFolder());
    ex.insert(QStringLiteral("title"), tr("Примеры"));
    ex.insert(QStringLiteral("subtitle"), tr("Встроенные: voln, horn, plasma_nby, vfield, eps4, xr, lens"));
    ex.insert(QStringLiteral("kind"), QStringLiteral("examples"));
    ex.insert(QStringLiteral("available"), true);
    out.append(ex);
    const QStringList recent = m_settings->value(QStringLiteral("folders/recent")).toStringList();
    for (const QString &f : recent) {
        QVariantMap m;
        const bool saf = Platform::isContentUri(f);
        // TAMIC_PHONE_LOOK — снимки руководства на ПК: папка подписана, как на телефоне.
        const bool phoneLook = saf || qEnvironmentVariableIsSet("TAMIC_PHONE_LOOK");
        m.insert(QStringLiteral("id"), f);
        m.insert(QStringLiteral("title"), folderTitle(f));
        m.insert(QStringLiteral("subtitle"), phoneLook ? tr("Папка на телефоне")
                                                       : QDir::toNativeSeparators(f));
        m.insert(QStringLiteral("kind"), saf ? QStringLiteral("saf") : QStringLiteral("local"));
        m.insert(QStringLiteral("available"), saf ? Platform::hasAccess(f) : QFileInfo(f).isDir());
        out.append(m);
    }
    return out;
}

void TaskStore::addRecent(const QString &folder)
{
    QStringList recent = m_settings->value(QStringLiteral("folders/recent")).toStringList();
    recent.removeAll(folder);
    recent.prepend(folder);
    while (recent.size() > 12)
        recent.removeLast();
    m_settings->setValue(QStringLiteral("folders/recent"), recent);
    if (Platform::isContentUri(folder))
        m_settings->setValue(QStringLiteral("folders/title/") + taskKey(folder, QString()),
                             Platform::folderTitle(folder));
    emit foldersChanged();
}

void TaskStore::pickFolder()
{
    Platform::instance()->pickFolder();
}

void TaskStore::addLocalFolder(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty() || !QFileInfo(path).isDir())
        return;
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    addRecent(clean);
    emit folderAdded(clean);
}

void TaskStore::forgetFolder(const QString &folder)
{
    QStringList recent = m_settings->value(QStringLiteral("folders/recent")).toStringList();
    if (recent.removeAll(folder) > 0) {
        m_settings->setValue(QStringLiteral("folders/recent"), recent);
        if (Platform::isContentUri(folder))
            Platform::releaseAccess(folder);
        emit foldersChanged();
    }
}

QString TaskStore::folderTitle(const QString &folder) const
{
    if (folder == examplesFolder())
        return tr("Примеры");
    if (Platform::isContentUri(folder)) {
        const QString saved = m_settings->value(QStringLiteral("folders/title/")
                                                + taskKey(folder, QString())).toString();
        return saved.isEmpty() ? Platform::folderTitle(folder) : saved;
    }
    const QString name = QFileInfo(folder).fileName();
    return name.isEmpty() ? QDir::toNativeSeparators(folder) : name;
}

// ------------------------------------------------------------------ доступ к файлам

QList<FolderEntry> TaskStore::list(const QString &folder) const
{
    if (Platform::isContentUri(folder))
        return Platform::listFolder(folder);
    QList<FolderEntry> out;
    const QFileInfoList infos = QDir(folder).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                                           QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &fi : infos) {
        FolderEntry e;
        e.name = fi.fileName();
        e.size = fi.size();
        e.modifiedMs = fi.lastModified().toMSecsSinceEpoch();
        e.isDir = fi.isDir();
        out.append(e);
    }
    return out;
}

QByteArray TaskStore::read(const QString &folder, const QString &name, qint64 maxBytes) const
{
    if (Platform::isContentUri(folder))
        return Platform::readFile(folder, name, maxBytes);
    QFile f(localPath(folder, name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.read(maxBytes);
}

bool TaskStore::copyIn(const QString &folder, const QString &name, const QString &dst) const
{
    if (Platform::isContentUri(folder))
        return Platform::copyToLocal(folder, name, dst);
    QFile::remove(dst);
    return QFile::copy(localPath(folder, name), dst);
}

bool TaskStore::copyOut(const QString &src, const QString &folder, const QString &name) const
{
    if (Platform::isContentUri(folder))
        return Platform::copyFromLocal(src, folder, name);
    const QString dst = localPath(folder, name);
    if (QFileInfo(src).canonicalFilePath() == QFileInfo(dst).canonicalFilePath())
        return true;
    QFile::remove(dst);
    if (!QFile::copy(src, dst))
        return false;
    // QFile::copy переносит права файла рабочей папки (ядро пишет с 0600); в папке
    // задачи результаты — обычные файлы пользователя, их читают другие программы.
    QFile::setPermissions(dst, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup
                                   | QFile::ReadOther);
    return true;
}

QStringList TaskStore::inputsOf(const QString &folder, const QString &tpl) const
{
    QStringList names;
    for (const FolderEntry &e : list(folder))
        if (!e.isDir)
            names.append(e.name);
    QStringList result{ tpl };
    QStringList queue{ tpl };
    QSet<QString> seen{ tpl.toLower() };
    while (!queue.isEmpty()) {
        const QString cur = queue.takeFirst();
        // Задание — в CP1251 (И-4): так совпадут и русские имена файлов.
        const QString text = stripComments(decodeCp1251(read(folder, cur, kMaxTextBytes)));
        for (const QString &n : std::as_const(names)) {
            if (seen.contains(n.toLower()) || !mentions(text, n))
                continue;
            seen.insert(n.toLower());
            result.append(n);
            queue.append(n);
        }
    }
    return result;
}

QString TaskStore::taskText(const QString &folder, const QString &tpl) const
{
    QString all;
    for (const QString &f : inputsOf(folder, tpl))
        all += decodeCp1251(read(folder, f, kMaxTextBytes)) + QLatin1Char('\n');
    return all;
}

// ------------------------------------------------------------------ задачи

QString TaskStore::taskKey(const QString &folder, const QString &name)
{
    const QByteArray h = QCryptographicHash::hash((folder + QLatin1Char('|') + name).toUtf8(),
                                                  QCryptographicHash::Sha1).toHex();
    return QString::fromLatin1(h.left(12));
}

QString TaskStore::workDirFor(const QString &folder, const QString &task, const QString &mode)
{
    QString base = QFileInfo(task).completeBaseName();
    static const QRegularExpression bad(QStringLiteral("[^A-Za-z0-9_.-]"));
    base.replace(bad, QStringLiteral("_"));
    if (base.isEmpty())
        base = QStringLiteral("task");
    const QString name = base + QLatin1Char('_') + mode + QLatin1Char('_')
                         + taskKey(folder, task).left(8);
    return QDir(Platform::dataDir()).filePath(QStringLiteral("calc/") + name);
}

QVariantList TaskStore::tasks(const QString &folder)
{
    QVariantList out;
    for (const FolderEntry &e : list(folder)) {
        if (e.isDir || !isTpl(e.name))
            continue;
        QVariantMap m;
        m.insert(QStringLiteral("name"), e.name);
        m.insert(QStringLiteral("size"), e.size);
        m.insert(QStringLiteral("sizeText"), formatSize(e.size));
        const QString key = QStringLiteral("task/") + taskKey(folder, e.name) + QLatin1Char('/');
        m.insert(QStringLiteral("mode"), m_settings->value(key + QLatin1String("mode")).toString());
        m.insert(QStringLiteral("lastCode"), m_settings->value(key + QLatin1String("code"), -1).toInt());
        m.insert(QStringLiteral("lastMode"), m_settings->value(key + QLatin1String("lastMode")).toString());
        out.append(m);
    }
    return out;
}

QString TaskStore::decodeCp1251(const QByteArray &bytes)
{
    static const char16_t upper[64] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    };
    QString out;
    out.reserve(bytes.size());
    for (const char ch : bytes) {
        const auto b = static_cast<unsigned char>(ch);
        if (b < 0x80) {
            if (b == '\r' || b == 0x1A)   // CRLF -> LF; признак конца файла DOS (И-2)
                continue;
            out.append(QChar(b));
        } else if (b < 0xC0) {
            out.append(QChar(upper[b - 0x80]));
        } else {
            out.append(QChar(char16_t(0x0410 + (b - 0xC0))));
        }
    }
    return out;
}

QString TaskStore::tplText(const QString &folder, const QString &name)
{
    return decodeCp1251(read(folder, name, kMaxTextBytes));
}

QString TaskStore::suggestMode(const QString &text, QString *reason)
{
    // Признаки X-моды: блоки плазмы — концентрация электронов (_N), магнитное поле
    // (_B), частота соударений (_Y), в том числе из файла (FILE_N/_B/_Y). Их
    // понимает только ядро X (Include/Tmcgrviw.h, CTMCRTH_INDANBLCK_*STAT[NBY],
    // CTMCRTH_INDANBLCK_FILE[NBY]; макросы X-моды в Typerth.h). Однозначного
    // признака моды в .tpl нет (вопрос В-2) — это подсказка.
    static const QRegularExpression re(
        QStringLiteral("\\b((?:RECT|CIRCLE|POLYGON)_STAT_[NBY]|FILE_[NBY])\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const QString code = stripComments(text);
    QStringList found;
    auto it = re.globalMatch(code);
    while (it.hasNext()) {
        const QString k = it.next().captured(1).toUpper();
        if (!found.contains(k))
            found.append(k);
    }
    if (!found.isEmpty()) {
        if (reason)
            *reason = QObject::tr("в задании блоки плазмы: %1").arg(found.join(QStringLiteral(", ")));
        return QStringLiteral("X");
    }
    if (reason)
        *reason = QObject::tr("блоков плазмы (*_STAT_N, _B, _Y; FILE_N, _B, _Y) нет");
    return QStringLiteral("H");
}

QVariantMap TaskStore::modeInfo(const QString &folder, const QString &name)
{
    QString reason;
    const QString suggested = suggestMode(taskText(folder, name), &reason);
    const QString saved = m_settings->value(QStringLiteral("task/") + taskKey(folder, name)
                                            + QLatin1String("/mode")).toString();
    QVariantMap m;
    m.insert(QStringLiteral("suggested"), suggested);
    m.insert(QStringLiteral("reason"), reason);
    m.insert(QStringLiteral("saved"), saved);
    m.insert(QStringLiteral("mode"), saved.isEmpty() ? suggested : saved);
    return m;
}

void TaskStore::setMode(const QString &folder, const QString &name, const QString &mode)
{
    const QString m = mode.toUpper() == QLatin1String("X") ? QStringLiteral("X") : QStringLiteral("H");
    m_settings->setValue(QStringLiteral("task/") + taskKey(folder, name) + QLatin1String("/mode"), m);
    bump();
}

void TaskStore::recordRun(const QString &folder, const QString &name, const QString &mode,
                          int code, double seconds, const QStringList &resultFiles)
{
    const QString key = QStringLiteral("task/") + taskKey(folder, name) + QLatin1Char('/');
    m_settings->setValue(key + QLatin1String("code"), code);
    m_settings->setValue(key + QLatin1String("lastMode"), mode);
    m_settings->setValue(key + QLatin1String("seconds"), seconds);
    m_settings->setValue(key + QLatin1String("date"), QDateTime::currentDateTime());
    m_settings->setValue(key + QLatin1String("results"), resultFiles);
    m_settings->setValue(QStringLiteral("last/folder"), folder);
    m_settings->setValue(QStringLiteral("last/task"), name);
    // Список посчитанных задач (сравнение графиков, allRuns): новые первыми.
    QStringList runs = m_settings->value(QStringLiteral("runs/index")).toStringList();
    const QString id = folder + QLatin1Char('\n') + name;
    runs.removeAll(id);
    runs.prepend(id);
    while (runs.size() > 100)
        runs.removeLast();
    m_settings->setValue(QStringLiteral("runs/index"), runs);
    m_settings->sync();
    bump();
}

QVariantMap TaskStore::lastRun(const QString &folder, const QString &name)
{
    QVariantMap m;
    QString f = folder, n = name;
    if (f.isEmpty() || n.isEmpty()) {
        f = m_settings->value(QStringLiteral("last/folder")).toString();
        n = m_settings->value(QStringLiteral("last/task")).toString();
    }
    m.insert(QStringLiteral("folder"), f);
    m.insert(QStringLiteral("task"), n);
    if (f.isEmpty() || n.isEmpty())
        return m;
    const QString key = QStringLiteral("task/") + taskKey(f, n) + QLatin1Char('/');
    m.insert(QStringLiteral("folderTitle"), folderTitle(f));
    m.insert(QStringLiteral("code"), m_settings->value(key + QLatin1String("code"), -1).toInt());
    m.insert(QStringLiteral("mode"), m_settings->value(key + QLatin1String("lastMode")).toString());
    m.insert(QStringLiteral("seconds"), m_settings->value(key + QLatin1String("seconds")).toDouble());
    m.insert(QStringLiteral("date"), m_settings->value(key + QLatin1String("date")).toDateTime()
                                         .toString(QStringLiteral("dd.MM.yyyy HH:mm")));
    return m;
}

QVariantList TaskStore::results(const QString &folder, const QString &name)
{
    QVariantList out;
    const QStringList files = m_settings->value(QStringLiteral("task/") + taskKey(folder, name)
                                                + QLatin1String("/results")).toStringList();
    if (files.isEmpty())
        return out;
    QSet<QString> want;
    for (const QString &f : files)
        want.insert(f.toLower());
    for (const FolderEntry &e : list(folder)) {
        if (e.isDir || !want.contains(e.name.toLower()))
            continue;
        QVariantMap m;
        m.insert(QStringLiteral("name"), e.name);
        m.insert(QStringLiteral("size"), e.size);
        m.insert(QStringLiteral("sizeText"), formatSize(e.size));
        m.insert(QStringLiteral("type"), QFileInfo(e.name).suffix().toUpper());
        out.append(m);
    }
    return out;
}

bool TaskStore::share(const QString &folder, const QStringList &names)
{
    return Platform::share(folder, names);
}

bool TaskStore::saveFile(const QString &localPath, const QString &folder, const QString &name)
{
    if (localPath.isEmpty() || folder.isEmpty() || name.isEmpty())
        return false;
    const bool ok = copyOut(localPath, folder, name);
    Platform::log(QStringLiteral("saveFile %1 -> %2: %3").arg(name, folder, ok ? QStringLiteral("ok") : QStringLiteral("fail")));
    return ok;
}

QStringList TaskStore::fileNames(const QString &folder) const
{
    QStringList out;
    for (const FolderEntry &e : list(folder))
        if (!e.isDir)
            out << e.name;
    return out;
}

QVariantMap TaskStore::deleteResults(const QString &folder, const QString &task, const QStringList &names)
{
    const QString key = QStringLiteral("task/") + taskKey(folder, task) + QLatin1String("/results");
    QStringList results = m_settings->value(key).toStringList();
    // Задание и всё, на что оно ссылается, не трогаем ни при каких условиях.
    QSet<QString> keep;
    for (const QString &f : inputsOf(folder, task))
        keep.insert(f.toLower());
    QSet<QString> known;
    for (const QString &f : std::as_const(results))
        known.insert(f.toLower());
    QHash<QString, qint64> sizes;
    for (const FolderEntry &e : list(folder))
        sizes.insert(e.name.toLower(), e.size);

    int deleted = 0, failed = 0;
    qint64 bytes = 0;
    for (const QString &n : names) {
        const QString low = n.toLower();
        if (!known.contains(low) || keep.contains(low) || isTpl(n))
            continue;
        bool ok;
        if (Platform::isContentUri(folder))
            ok = Platform::deleteFile(folder, n);
        else
            ok = !QFileInfo::exists(localPath(folder, n)) || QFile::remove(localPath(folder, n));
        if (ok) {
            ++deleted;
            bytes += sizes.value(low);
            results.removeIf([&low](const QString &r) { return r.toLower() == low; });
        } else {
            ++failed;
        }
    }
    m_settings->setValue(key, results);
    if (results.isEmpty()) {
        // Копии расчёта во внутренней памяти тоже больше не нужны.
        for (const QString &mode : { QStringLiteral("H"), QStringLiteral("X") })
            QDir(workDirFor(folder, task, mode)).removeRecursively();
        QDir(QDir(Platform::dataDir()).filePath(QStringLiteral("view/") + taskKey(folder, task)))
            .removeRecursively();
    }
    m_settings->sync();
    Platform::log(QStringLiteral("deleteResults %1: deleted=%2 failed=%3").arg(task).arg(deleted).arg(failed));
    bump();
    QVariantMap m;
    m.insert(QStringLiteral("deleted"), deleted);
    m.insert(QStringLiteral("failed"), failed);
    m.insert(QStringLiteral("bytes"), bytes);
    return m;
}

QVariantList TaskStore::allRuns()
{
    QStringList ids = m_settings->value(QStringLiteral("runs/index")).toStringList();
    // Расчёты до появления списка: хотя бы последний.
    const QString lastFolder = m_settings->value(QStringLiteral("last/folder")).toString();
    const QString lastTask = m_settings->value(QStringLiteral("last/task")).toString();
    if (!lastFolder.isEmpty() && !lastTask.isEmpty()) {
        const QString lastId = lastFolder + QLatin1Char('\n') + lastTask;
        if (!ids.contains(lastId))
            ids.append(lastId);
    }
    QVariantList out;
    for (const QString &id : std::as_const(ids)) {
        const qsizetype sep = id.indexOf(QLatin1Char('\n'));
        if (sep <= 0)
            continue;
        const QString folder = id.left(sep), task = id.mid(sep + 1);
        if (Platform::isContentUri(folder) ? !Platform::hasAccess(folder) : !QFileInfo(folder).isDir())
            continue;
        const QVariantList files = results(folder, task);
        if (files.isEmpty())
            continue;
        QStringList names;
        for (const QVariant &f : files)
            names << f.toMap().value(QStringLiteral("name")).toString();
        const QVariantMap run = lastRun(folder, task);
        QVariantMap m;
        m.insert(QStringLiteral("folder"), folder);
        m.insert(QStringLiteral("folderTitle"), folderTitle(folder));
        m.insert(QStringLiteral("task"), task);
        m.insert(QStringLiteral("date"), run.value(QStringLiteral("date")));
        m.insert(QStringLiteral("files"), names);
        out.append(m);
    }
    return out;
}

QString TaskStore::viewFolder(const QString &folder, const QString &task, const QStringList &names)
{
    if (!Platform::isContentUri(folder))
        return folder;
    QHash<QString, qint64> sizes;
    for (const FolderEntry &e : list(folder))
        sizes.insert(e.name, e.size);
    auto sameFiles = [&](const QString &dir) {
        for (const QString &n : names) {
            const QFileInfo fi(QDir(dir).filePath(n));
            if (!fi.exists() || fi.size() != sizes.value(n, -1))
                return false;
        }
        return true;
    };
    // Рабочая папка последнего расчёта — те же файлы без копирования.
    const QString mode = lastRun(folder, task).value(QStringLiteral("mode")).toString();
    if (!mode.isEmpty()) {
        const QString work = workDirFor(folder, task, mode);
        if (sameFiles(work))
            return work;
    }
    const QString dir = QDir(Platform::dataDir()).filePath(QStringLiteral("view/") + taskKey(folder, task));
    QDir().mkpath(dir);
    for (const QString &n : names) {
        const QFileInfo fi(QDir(dir).filePath(n));
        if (fi.exists() && fi.size() == sizes.value(n, -1))
            continue;
        if (!copyIn(folder, n, fi.filePath()))
            return QString();
    }
    return dir;
}

static qint64 dirSize(const QString &dir)
{
    qint64 n = 0;
    QDirIterator it(dir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        n += it.fileInfo().size();
    }
    return n;
}

QString TaskStore::tempPath(const QString &name) const
{
    const QString dir = QDir(Platform::dataDir()).filePath(QStringLiteral("export"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(name);
}

qint64 TaskStore::cacheSize() const
{
    const QDir data(Platform::dataDir());
    return dirSize(data.filePath(QStringLiteral("calc"))) + dirSize(data.filePath(QStringLiteral("view")))
           + dirSize(data.filePath(QStringLiteral("export")))
           + dirSize(QDir(Platform::externalDir()).filePath(QStringLiteral("promo")));
}

qint64 TaskStore::clearCache()
{
    const qint64 before = cacheSize();
    const QDir data(Platform::dataDir());
    QDir(data.filePath(QStringLiteral("calc"))).removeRecursively();
    QDir(data.filePath(QStringLiteral("view"))).removeRecursively();
    QDir(data.filePath(QStringLiteral("export"))).removeRecursively();
    QDir(QDir(Platform::externalDir()).filePath(QStringLiteral("promo"))).removeRecursively();   // кадры видео
    const qint64 freed = before - cacheSize();
    Platform::log(QStringLiteral("clearCache: %1 bytes").arg(freed));
    bump();
    return freed;
}

QString TaskStore::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return tr("%1 Б").arg(bytes);
    if (bytes < 1024 * 1024)
        return tr("%1 КБ").arg(viewers::localizeNumber(QString::number(double(bytes) / 1024.0, 'f', 1)));
    if (bytes < 1024LL * 1024 * 1024)
        return tr("%1 МБ").arg(viewers::localizeNumber(QString::number(double(bytes) / (1024.0 * 1024.0), 'f', 1)));
    return tr("%1 ГБ").arg(viewers::localizeNumber(QString::number(double(bytes) / (1024.0 * 1024.0 * 1024.0), 'f', 2)));
}
