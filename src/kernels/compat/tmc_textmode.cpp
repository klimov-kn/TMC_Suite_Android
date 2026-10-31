// tmc_textmode.cpp — текстовый режим файлов и поиск имени, как в Windows.
//
// ИСТОЧНИК: src/compat/tmc_textmode.cpp версии для macOS,
// перенесён без изменения логики. Добавлено для Android (bionic, этап П1): в
// bionic НЕТ fopencookie (в заголовках NDK r27 его нет вовсе), но есть funopen
// с той же подписью, что в Darwin (fpos_t в bionic — off_t, целое; ISSUES И-4),
// поэтому Android идёт по ветке macOS (TMC_TEXTMODE_FUNOPEN).
//
// ЗАЧЕМ. Файлы заданий и текстовые результаты TMC записаны с окончаниями строк
// CRLF. На Windows `fopen(..., "r")` открывает поток в ТЕКСТОВОМ режиме: при
// чтении пара CRLF превращается в один перевод строки, при записи наоборот.
// На Linux и на macOS текстовый режим ничем не отличается от двоичного, поэтому
// символ возврата каретки доживает до разбора: пустая строка перестаёт быть
// пустой, число «20.0344\r» читается неправильно, а записанный файл отличается
// от windows-версии на байты окончаний строк — то есть перестаёт быть
// совместимым.
//
// Вторая задача — имя файла. Файловая система Windows не различает регистр:
// задание подключает `#include horns.h`, а на диске лежит `HORNS.H`. На ext4 и
// на томе APFS, размеченном с учётом регистра, это разные имена, и файл,
// созданный на Windows, не открывается.
//
// РЕШЕНИЕ. Открытие файлов перехватывается ПРЕПРОЦЕССОРОМ, а не компоновщиком:
// преамбула win_src (compat/include/tmc_win_prelude.h) объявляет
// `#define fopen tmc_fopen`, и весь код Windows-версии приходит сюда. Раньше
// подмена делалась ключом `-Wl,--wrap=fopen`, но компоновщик Apple такого ключа
// не понимает, а один способ на обе системы лучше двух.
// Для ТЕКСТОВЫХ режимов (без 'b') возвращаем поток, который при чтении убирает
// возврат каретки, а при записи ставит его перед каждым переводом строки.
// ДВОИЧНЫЕ режимы идут в настоящий fopen без изменений — числовые форматы
// (S-матрицы) остаются байт в байт теми же. Имя в обоих случаях разрешается
// без учёта регистра.
//
// ПОТОК СО СВОИМИ ФУНКЦИЯМИ ОБМЕНА. В glibc такой поток создаёт `fopencookie`,
// в libc Darwin этого вызова нет — там его BSD-предшественник `funopen`.
// Отличаются только подписи (у funopen размеры и результат обмена — `int`,
// смещение — `fpos_t`), поэтому для macOS написаны тонкие переходники, а сама
// обработка блоков — textReadBlock/textWriteBlock — общая.
//
// ПРОИЗВОДИТЕЛЬНОСТЬ. Перевод строк делается БЛОКАМИ, а не посимвольно: счётные
// ядра пишут выходной файл на каждом такте, и файлы бывают под мегабайт.
// Посимвольная обработка (fgetc/fputc на каждый байт) замедляла счёт с двух
// секунд до десяти минут. Дублировать обработку ради второй системы нельзя по
// той же причине: расходиться будет не только скорость, но и поведение.
//
// Это слой совместимости, а не вычисления: меняется только трактовка окончаний
// строк на границе ввода-вывода — ровно то, что делает библиотека C на Windows.
//
// ТРЕТЬЯ ЗАДАЧА (добавлено для приложения, решение M26, ISSUES П-10) — файл
// мгновенного поля (.ex/.ey/.hz). Ядро переписывает его каждые полтакта (PutField:
// fopen "wb", кадр, fclose) и держит открытым почти всё время, так что читатель
// застаёт его недописанным. Поэтому кадр пишется во временный файл рядом
// («<имя>.tmp~»), а при fclose заменяет собой файл поля переименованием — на одном
// томе это атомарно: читатель (FieldRecorder, экран «Поле») в любой момент видит
// цельный предыдущий или новый кадр, а ядро никого не ждёт. Байты кадра те же.

#ifndef _WIN32

// Преамбула win_src подменяет fopen макросом. Внутри ЭТОГО файла нужен
// настоящий fopen, иначе обёртка позвала бы сама себя. Снятие подмены стоит
// до всех включений и работает в любом случае: даже если преамбулу подставят
// ключом -include (он разворачивается перед первой строкой файла), макрос к
// этому месту уже снят. Вариант «просто не подключать преамбулу» надёжным не
// является — состав ключей у целей меняется, а эта строка не зависит ни от чего.
#undef fopen
#undef remove

#if defined(__APPLE__) || defined(__ANDROID__)
  #define TMC_TEXTMODE_FUNOPEN 1
#endif

#if !defined(TMC_TEXTMODE_FUNOPEN) && !defined(_GNU_SOURCE)
  // fopencookie и off64_t — расширения glibc, объявляются только под _GNU_SOURCE.
  #define _GNU_SOURCE
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace {

// --- Файл мгновенного поля: запись с атомарной заменой -----------------------------

bool isFieldFile(const char* path)
{
    const char* dot = std::strrchr(path, '.');
    const char* slash = std::strrchr(path, '/');
    if (!dot || (slash && dot < slash))
        return false;
    return strcasecmp(dot, ".ex") == 0 || strcasecmp(dot, ".ey") == 0 || strcasecmp(dot, ".hz") == 0;
}


// --- Поиск файла без учёта регистра -------------------------------------------
//
// Ищем только по последнему элементу пути и только когда точного имени нет.
// Если совпадений несколько, ничего не выбираем — пусть лучше будет честная
// ошибка, чем открытие не того файла.
std::string resolveIgnoringCase(const char* path)
{
    if (!path || !*path)
        return std::string();

    struct stat st;
    if (::stat(path, &st) == 0)
        return path;                       // точное имя есть — ничего не ищем

    std::string full(path);

    // Внутри заданий, созданных на Windows, вложенные файлы записаны через
    // обратную косую (sub\\horns.h). Это содержимое файла задания — менять его
    // нельзя, поэтому разделитель приводим здесь, при поиске. Windows принимала
    // оба варианта, macOS считает обратную косую обычным символом имени.
    if (full.find('\\') != std::string::npos) {
        std::string converted = full;
        for (std::string::size_type i = 0; i < converted.size(); ++i) {
            if (converted[i] == '\\')
                converted[i] = '/';
        }
        if (::stat(converted.c_str(), &st) == 0)
            return converted;
        full = converted;      // дальше ищем без учёта регистра уже по нему
    }

    const std::string::size_type slash = full.find_last_of('/');
    const std::string dir  = (slash == std::string::npos) ? std::string(".")
                                                          : full.substr(0, slash);
    const std::string name = (slash == std::string::npos) ? full
                                                          : full.substr(slash + 1);
    if (name.empty())
        return full;

    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return full;

    std::string found;
    int matches = 0;
    while (struct dirent* e = ::readdir(d)) {
        if (::strcasecmp(e->d_name, name.c_str()) == 0) {
            ++matches;
            found = e->d_name;
        }
    }
    ::closedir(d);

    if (matches != 1)
        return full;                       // ноль или несколько — не гадаем

    return (slash == std::string::npos) ? found : dir + "/" + found;
}

// --- Поток с переводом окончаний строк ----------------------------------------

struct TextStream
{
    FILE* raw;          // настоящий поток, открытый в двоичном режиме
    char* buffer;       // рабочий буфер для блочного обмена
    size_t bufferSize;
};

const size_t kBufferSize = 64 * 1024;

// Обработка блоков — общая для обеих систем. Подписи выбраны под glibc
// (cookie_read_function_t и его соседи), потому что там они шире: под них
// переходники для funopen пишутся сужением, а не наоборот.

// Чтение: берём блок и выбрасываем из него возвраты каретки. Если после чистки
// не осталось ни байта (блок был из одних CR), читаем следующий блок.
ssize_t textReadBlock(void* cookie, char* buf, size_t size)
{
    TextStream* ts = static_cast<TextStream*>(cookie);
    if (size == 0)
        return 0;

    for (;;) {
        const size_t want = size < ts->bufferSize ? size : ts->bufferSize;
        const size_t got = std::fread(ts->buffer, 1, want, ts->raw);
        if (got == 0)
            return std::ferror(ts->raw) ? -1 : 0;

        // Быстрый путь: возвратов каретки в блоке нет — отдаём как есть.
        const char* cr = static_cast<const char*>(std::memchr(ts->buffer, '\r', got));
        if (!cr) {
            std::memcpy(buf, ts->buffer, got);
            return static_cast<ssize_t>(got);
        }

        size_t out = 0;
        const char* src = ts->buffer;
        const char* end = ts->buffer + got;
        while (src < end) {
            const char* next = static_cast<const char*>(
                std::memchr(src, '\r', size_t(end - src)));
            if (!next) {
                const size_t n = size_t(end - src);
                std::memcpy(buf + out, src, n);
                out += n;
                break;
            }
            const size_t n = size_t(next - src);
            std::memcpy(buf + out, src, n);
            out += n;
            src = next + 1;                 // сам возврат каретки пропускаем
        }
        if (out > 0)
            return static_cast<ssize_t>(out);
        // Блок состоял из одних возвратов каретки — читаем следующий.
    }
}

// Запись: перед каждым переводом строки добавляем возврат каретки. Пишем
// кусками между переводами строк, а не по байту.
ssize_t textWriteBlock(void* cookie, const char* buf, size_t size)
{
    TextStream* ts = static_cast<TextStream*>(cookie);
    size_t done = 0;

    while (done < size) {
        const char* start = buf + done;
        const size_t left = size - done;
        const char* nl = static_cast<const char*>(std::memchr(start, '\n', left));
        const size_t chunk = nl ? size_t(nl - start) : left;

        if (chunk && std::fwrite(start, 1, chunk, ts->raw) != chunk)
            return -1;
        done += chunk;

        if (nl) {
            if (std::fwrite("\r\n", 1, 2, ts->raw) != 2)
                return -1;
            done += 1;                      // сам перевод строки уже записан
        }
    }
    return static_cast<ssize_t>(size);
}

int textCloseStream(void* cookie)
{
    TextStream* ts = static_cast<TextStream*>(cookie);
    int r = std::fclose(ts->raw);
    std::free(ts->buffer);
    std::free(ts);
    return r;
}

// Поток кадра поля: пишет во временный файл, при закрытии переименовывает его в файл
// поля (см. начало файла). Чтения и перемотки у кадра нет — ядро пишет его подряд.
struct FieldStream
{
    FILE* raw;
    std::string tmp;
    std::string path;
};

ssize_t fieldWrite(void* cookie, const char* buf, size_t size)
{
    FieldStream* fs = static_cast<FieldStream*>(cookie);
    return std::fwrite(buf, 1, size, fs->raw) == size ? static_cast<ssize_t>(size) : -1;
}

int fieldClose(void* cookie)
{
    FieldStream* fs = static_cast<FieldStream*>(cookie);
    int r = std::fclose(fs->raw);
    if (r == 0 && std::rename(fs->tmp.c_str(), fs->path.c_str()) != 0)
        r = EOF;
    if (r != 0)
        std::remove(fs->tmp.c_str());
    delete fs;
    return r;
}

#ifdef TMC_TEXTMODE_FUNOPEN
int fieldWriteApple(void* cookie, const char* buf, int size)
{
    if (size <= 0)
        return 0;
    return static_cast<int>(fieldWrite(cookie, buf, static_cast<size_t>(size)));
}
#endif

FILE* openFieldForWrite(const char* path)
{
    FieldStream* fs = new FieldStream{ 0, std::string(path) + ".tmp~", std::string(path) };
    fs->raw = std::fopen(fs->tmp.c_str(), "wb");
    if (!fs->raw) {
        delete fs;
        return 0;
    }
#ifdef TMC_TEXTMODE_FUNOPEN
    FILE* f = ::funopen(fs, 0, fieldWriteApple, 0, fieldClose);
#else
    cookie_io_functions_t io = { 0, fieldWrite, 0, fieldClose };
    FILE* f = fopencookie(fs, "wb", io);
#endif
    if (!f) {
        std::fclose(fs->raw);
        std::remove(fs->tmp.c_str());
        delete fs;
        return 0;
    }
    std::setvbuf(f, 0, _IOFBF, 1 << 16);
    return f;
}

// Позиционирование отдаём настоящему потоку. Смещения в файле с CRLF отличаются
// от «логических» на число возвратов каретки, но текстовые файлы TMC читаются
// строго последовательно (fgets/fscanf), а там, где нужен точный отсчёт байтов
// (S-матрицы), файл открывается в двоичном режиме и сюда не попадает.

#ifdef TMC_TEXTMODE_FUNOPEN

// Переходники под funopen. Отличия от glibc только в типах: размер и результат
// обмена — int, смещение — fpos_t (в Darwin это знаковое 64-разрядное целое,
// арифметика с ним обычная). Логика перевода строк здесь не повторяется.
int textReadApple(void* cookie, char* buf, int size)
{
    if (size <= 0)
        return 0;
    return static_cast<int>(textReadBlock(cookie, buf, static_cast<size_t>(size)));
}

int textWriteApple(void* cookie, const char* buf, int size)
{
    if (size <= 0)
        return 0;
    return static_cast<int>(textWriteBlock(cookie, buf, static_cast<size_t>(size)));
}

fpos_t textSeekApple(void* cookie, fpos_t offset, int whence)
{
    TextStream* ts = static_cast<TextStream*>(cookie);
    if (fseeko(ts->raw, static_cast<off_t>(offset), whence) != 0)
        return static_cast<fpos_t>(-1);
    return static_cast<fpos_t>(ftello(ts->raw));
}

#else

int textSeek(void* cookie, off64_t* offset, int whence)
{
    TextStream* ts = static_cast<TextStream*>(cookie);
    if (fseeko(ts->raw, static_cast<off_t>(*offset), whence) != 0)
        return -1;
    *offset = static_cast<off64_t>(ftello(ts->raw));
    return 0;
}

#endif // TMC_TEXTMODE_FUNOPEN

} // namespace

// Открытие файла вместо fopen: имя подставляет преамбула win_src, поэтому весь
// код Windows-версии приходит именно сюда. Внутри — настоящий std::fopen,
// подмена в этом файле снята.
extern "C" FILE* tmc_fopen(const char* path, const char* mode)
{
    if (mode == 0)
        return std::fopen(path, mode);

    // Имя разрешаем так же, как это делает Windows: без учёта регистра. Это
    // касается и записи: если файл с таким именем уже есть (пусть и в другом
    // регистре), Windows пишет именно в него, а не заводит второй.
    const std::string resolved = resolveIgnoringCase(path);
    const char* realPath = resolved.empty() ? path : resolved.c_str();

    // Двоичный режим — без перевода строк. Кадр поля — через временный файл (см. начало).
    if (std::strcmp(mode, "wb") == 0 && isFieldFile(realPath))
        return openFieldForWrite(realPath);
    if (std::strchr(mode, 'b'))
        return std::fopen(realPath, mode);

    // Текстовый режим: настоящий поток открываем двоичным, перевод строк берёт
    // на себя обёртка.
    char rawMode[8];
    std::snprintf(rawMode, sizeof(rawMode), "%sb", mode);

    FILE* raw = std::fopen(realPath, rawMode);
    if (!raw)
        return 0;

    TextStream* ts = static_cast<TextStream*>(std::malloc(sizeof(TextStream)));
    if (!ts) {
        std::fclose(raw);
        return 0;
    }
    ts->raw = raw;
    ts->bufferSize = kBufferSize;
    ts->buffer = static_cast<char*>(std::malloc(ts->bufferSize));
    if (!ts->buffer) {
        std::fclose(raw);
        std::free(ts);
        return 0;
    }

#ifdef TMC_TEXTMODE_FUNOPEN
    // funopen узнаёт направление обмена не из строки режима — её он вообще не
    // получает, — а из того, какие функции заданы. Поэтому режим разбираем сами:
    // 'r' — только чтение, 'w'/'a' — только запись, знак '+' добавляет второе.
    // Если оставить обе, поток «только для чтения» молчаливо разрешил бы запись.
    const bool plus      = std::strchr(mode, '+') != 0;
    const bool wantRead  = (mode[0] == 'r') || plus;
    const bool wantWrite = (mode[0] != 'r') || plus;

    FILE* f = ::funopen(ts,
                        wantRead  ? textReadApple  : 0,
                        wantWrite ? textWriteApple : 0,
                        textSeekApple,
                        textCloseStream);
#else
    cookie_io_functions_t io = { textReadBlock, textWriteBlock,
                                 textSeek, textCloseStream };
    FILE* f = fopencookie(ts, mode, io);
#endif
    if (!f) {
        std::fclose(raw);
        std::free(ts->buffer);
        std::free(ts);
        return 0;
    }

    // Свой буфер и у внешнего потока: иначе каждый fputs из кода превращается
    // в отдельный вызов обёртки.
    std::setvbuf(f, 0, _IOFBF, kBufferSize);
    return f;
}

// Удаление файла вместо remove (добавлено для Android, этап П1): то же
// разрешение имени, что в tmc_fopen (обратная косая, регистр).
extern "C" int tmc_remove(const char* path)
{
    const std::string resolved = resolveIgnoringCase(path);
    return std::remove(resolved.empty() ? path : resolved.c_str());
}

// GetFileAttributes для кода расчёта (этап 1B, опись J): ядро спрашивает, прочитал ли
// внешний FieldView файл поля (TmcRTHRectNode.cpp, IsFieldFileRead). Путь — в записи
// Windows (П-2), поэтому имя разрешается так же, как в tmc_fopen. Ответ — как у только
// что записанного файла в Windows: есть -> FILE_ATTRIBUTE_ARCHIVE (0x20), нет -> 0xFFFFFFFF.
// SetFileAttributes атрибутов не хранит (FieldView — экран того же приложения, В-15),
// поэтому ожидания чтения поля внешней программой вне Windows не бывает.
extern "C" unsigned long tmc_get_file_attributes(const char* path)
{
    if (!path)
        return 0xFFFFFFFFul;
    const std::string resolved = resolveIgnoringCase(path);
    struct stat st;
    return (::stat(resolved.empty() ? path : resolved.c_str(), &st) == 0) ? 0x20ul : 0xFFFFFFFFul;
}

#endif // !_WIN32
