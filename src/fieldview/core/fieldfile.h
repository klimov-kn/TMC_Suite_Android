/// @file fieldfile.h
/// @brief Чтение файлов поля ядер TAMIC (FIELDS -> <имя>.ex, FIELD_DISTRIBUTION -> .AMP/.FAZ).
///
/// Формат и разбор перенесены без изменений из Windows-версии FieldView
/// (src/win_src/fieldview/FldViewDoc.cpp: ReadDataParameters, ReadDataArray):
///
///   #TMC_GraphicsOutputFieldFile FormVer2.0 2000 \r\n
///   #TMCGROFLD_nT <int>\r\n          #TMCGROFLD_Time <double>\r\n
///   #TMCGROFLD_Delta <double>\r\n    #TMCGROFLD_Xmin <double>\r\n
///   #TMCGROFLD_Ymin <double>\r\n     #TMCGROFLD_Accuracy <4|8>\r\n
///   #TMCGROFLD_LongUnit <s> <double>\r\n  #TMCGROFLD_TimeUnit <s> <double>\r\n
///   #TMCGROFLD_nX <int>\r\n          #TMCGROFLD_nY <int>
///   затем nY строк: \r\n + nX значений float/double (порядок байт машины, x86/arm64 — LE).
///
/// Строки заголовка сверяются по началу (strncmp), числа берутся как sscanf %d / %lg —
/// но без зависимости от локали (на Android Qt ставит локаль системы, а sscanf с
/// русской локалью ждёт запятую).
///
/// «Серия тактов» <имя>.ex.frames — НЕ формат ядра: это подряд записанные копии файла
/// .ex, каждая байт в байт как у ядра (FieldRecorder). Один файл .ex —
/// серия из одного такта.
/// @see https://www.tamic.ru/docs/platforms/android.html#field

#pragma once

#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

namespace fieldview {

/// Параметры одного кадра (такта) — заголовок файла поля.
struct FieldHeader
{
    int nT = 0;              // номер такта
    double time = 0.0;       // модельное время, с
    double delta = 0.001;    // шаг сетки, м
    double xMin = 0.0;
    double yMin = 0.0;
    int accuracy = 8;        // байт на значение: 4 (float) или 8 (double)
    QString longUnit = QStringLiteral("m");
    double longUnitValue = 1.0;
    QString timeUnit = QStringLiteral("s");
    double timeUnitValue = 1.0;
    int nX = 0;
    int nY = 0;
};

/// Значения кадра и их размах — как в CFldViewView::PrepareData.
struct FieldStats
{
    double min = 0.0;        // минимум значения
    double max = 0.0;        // максимум значения
    double maxAbs = 0.0;     // максимум модуля
};

/// Кадр в памяти: значения узлов (nX*nY, строка за строкой, как pdSurface) во float
/// для видеокарты; размах считан по исходным числам (double).
struct FieldFrame
{
    FieldHeader header;
    std::vector<float> values;
    FieldStats stats;
};

/// Разбор заголовка из буфера. Возвращает смещение начала данных или -1 и текст ошибки
/// (тексты ошибок — как в оригинале: "bad data file" …).
qint64 parseFieldHeader(const char *data, qint64 size, FieldHeader *out, QString *error);

/// Размер данных кадра после заголовка: nY строк по nX значений, между строками \r\n.
qint64 fieldDataBytes(const FieldHeader &h);

/// Разбор значений (ReadDataArray) и размаха. data указывает на начало данных.
bool decodeFieldValues(const char *data, qint64 size, const FieldHeader &h, FieldFrame *out,
                       QString *error);

/// Прочитать один файл поля целиком (как CFldViewDoc::ReadData).
bool readFieldFile(const QString &path, FieldFrame *out, QString *error);

/// Серия тактов: один .ex или склейка .ex.frames. Кадры читаются по требованию.
class FieldSeries
{
public:
    struct Entry
    {
        qint64 offset = 0;       // начало заголовка кадра в файле
        qint64 dataOffset = 0;   // начало значений
        FieldHeader header;
    };

    bool open(const QString &path, QString *error);
    void close();

    QString path() const { return m_path; }
    int count() const { return int(m_entries.size()); }
    const Entry &entry(int i) const { return m_entries[size_t(i)]; }
    /// Сетка одна на всю серию (кадры с другой сеткой отбрасываются при открытии).
    int nX() const { return m_entries.empty() ? 0 : m_entries.front().header.nX; }
    int nY() const { return m_entries.empty() ? 0 : m_entries.front().header.nY; }

    /// Прочитать кадр i с диска.
    bool readFrame(int i, FieldFrame *out, QString *error) const;

private:
    QString m_path;
    std::vector<Entry> m_entries;
};

} // namespace fieldview
