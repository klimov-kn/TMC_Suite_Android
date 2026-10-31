/// @file tmc_textmode.cpp
/// @brief Открытие файлов кодом расчёта: текстовый режим Windows, имена без учёта регистра, кадры поля.
///
/// Файлы заданий и текстовые результаты записаны с окончаниями CRLF, а Windows не различает
/// регистр имён. Преамбула `tmc_win_prelude.h` подменяет `fopen` и `remove` на `tmc_fopen` и
/// `tmc_remove`, и весь код расчёта приходит сюда: текстовые режимы получают поток с переводом
/// окончаний строк блоками, двоичные идут в настоящий `fopen`. На Android поток создаёт `funopen`
/// (в bionic нет `fopencookie`), на Linux — `fopencookie`.
///
/// Только в версии для Android: кадр мгновенного поля (`.ex`, `.ey`, `.hz`) пишется во временный
/// файл и при закрытии заменяет файл поля переименованием. Экран «Поле» поэтому всегда читает
/// цельный кадр, а ядро никого не ждёт.
/// @see https://www.tamic.ru/docs/platforms/android.html#kernels

// Преамбула win_src подменяет fopen макросом. Внутри этого файла нужен
// настоящий fopen, иначе обёртка позвала бы сама себя. Снятие подмены стоит
// до всех включений и работает в любом случае: даже если преамбулу подставят
// ключом -include (он разворачивается перед первой строкой файла), макрос к
// этому месту уже снят. Вариант «просто не подключать преамбулу» надёжным не
// является — состав ключей у целей меняется, а эта строка не зависит ни от чего.
#undef fopen
#undef remove

#if defined(__ANDROID__)
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
    // Данные отдаём системе сразу: ядро держит .t открытым весь шаг и читает этот же файл другим
    // потоком сразу после fflush. Лишних обращений к диску нет: функция вызывается, только когда
    // заполнен буфер внешнего потока или по fflush/fclose.
    if (std::fflush(ts->raw) != 0)
        return -1;
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
int fieldWriteFunopen(void* cookie, const char* buf, int size)
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
    FILE* f = ::funopen(fs, 0, fieldWriteFunopen, 0, fieldClose);
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
int textReadFunopen(void* cookie, char* buf, int size)
{
    if (size <= 0)
        return 0;
    return static_cast<int>(textReadBlock(cookie, buf, static_cast<size_t>(size)));
}

int textWriteFunopen(void* cookie, const char* buf, int size)
{
    if (size <= 0)
        return 0;
    return static_cast<int>(textWriteBlock(cookie, buf, static_cast<size_t>(size)));
}

fpos_t textSeekFunopen(void* cookie, fpos_t offset, int whence)
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
                        wantRead  ? textReadFunopen  : 0,
                        wantWrite ? textWriteFunopen : 0,
                        textSeekFunopen,
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
