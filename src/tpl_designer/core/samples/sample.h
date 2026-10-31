#pragma once

#include "project.h"

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

/// Библиотека образцов: готовые СВЧ-устройства и задачи, которые строятся по параметрам.
///
/// Образец — это РЕЦЕПТ, а не файл: функция, которая по выбранному сечению, частоте и
/// размерам собирает `tpl::Project`. Так любой выбор даёт гарантированно корректное
/// задание (его проверяет тот же `validate()`, что и мастер), а тридцать сечений
/// волновода — это тридцать строк таблицы, а не тридцать файлов.
///
/// Здесь нет ни одного виджета: рецепты тестируются без GUI, как и весь `tpl_core`.
namespace tpl::samples {

/// Поле карточки образца.
struct Param {
    enum Kind { Number, Integer, Choice, Flag };

    QString key;
    QString label;
    QString unit;   ///< «мм», «ГГц», «град.» — показывается рядом с полем
    QString hint;   ///< подсказка под курсором
    Kind kind = Number;
    double def = 0.0;
    double min = 0.0;
    double max = 0.0;
    double step = 1.0;
    int decimals = 2;
    QStringList choices; ///< для Choice; значение — номер строки

    static Param number(const QString &key, const QString &label, const QString &unit,
                        double def, double min, double max, double step, int decimals,
                        const QString &hint = {});
    static Param integer(const QString &key, const QString &label, int def, int min, int max,
                         const QString &hint = {});
    static Param choice(const QString &key, const QString &label, const QStringList &items,
                        int def, const QString &hint = {});
    static Param flag(const QString &key, const QString &label, bool def,
                      const QString &hint = {});
};

/// Значения параметров. Выбор — номер строки, флаг — 0 или 1.
using Values = QHash<QString, double>;

/// Строка паспорта: «Отсечка H10 — 6.56 ГГц».
struct Row {
    QString label;
    QString value;
};

/// Что даёт рецепт.
struct Build {
    Project project;
    /// Теория: то, что считается формулами при ЛЮБЫХ параметрах.
    QVector<Row> theory;
    /// Пояснения к постановке — что именно сделано и на что смотреть после счёта.
    QStringList notes;
    /// Файлы, которые обязаны лечь РЯДОМ с заданием (готовое распределение `.eps`).
    /// Имя — без пути; содержимое — байты как есть.
    QMap<QString, QByteArray> companions;
};

/// Раздел каталога.
struct Section {
    QString id;
    QString title;
    QString summary;
};

/// Образец.
struct Sample {
    QString id;       ///< латиницей, уникален: по нему живут числа ядра и тесты
    QString section;  ///< id раздела
    QString title;
    QString summary;  ///< одна строка для плитки каталога
    QString standard; ///< стандарт или источник: «ГОСТ 20900-75, IEC 60153-2, EIA RS-261»
    /// Справка: что это, как устроено, что смотреть в результатах. Пишется так, чтобы
    /// стать сценарием урока.
    QString help;
    Kernel kernel = Kernel::H;
    QVector<Param> params;
    std::function<Build(const Values &)> build;
};

/// Посчитанное ядром для параметров по умолчанию.
struct Baked {
    QString sampleId;
    QString when;          ///< дата прогона
    QString setup;         ///< сетка и время: «сетка 263x48, шаг 0.57 мм, 21 частота»
    QVector<Row> rows;     ///< числа из .s
};

const QVector<Section> &sections();
const QVector<Sample> &all();
const Sample *find(const QString &id);
QVector<const Sample *> inSection(const QString &sectionId);

/// Значения по умолчанию.
Values defaults(const Sample &s);
/// Равны ли значения умолчаниям — только тогда числа ядра относятся к карточке.
bool isDefault(const Sample &s, const Values &v);

/// Числа ядра для образца; nullptr — не считали.
const Baked *baked(const QString &sampleId);

/// Собрать образец. Параметры, которых нет в \p v, берутся по умолчанию.
Build make(const Sample &s, const Values &v);

// --- внутреннее: разделы регистрируют свои рецепты ---
void addWaveguides(QVector<Sample> *out);
void addAntennas(QVector<Sample> *out);
void addMicrostrip(QVector<Sample> *out);
/// Вторая половина раздела «Микрополосковые»: изгиб, делитель, нагрузка, разомкнутый
/// конец, импульсная рефлектометрия, линия задержки, тейпер, патч, ПЗФ на шлейфах.
/// Отдельным файлом только из-за размера — раздел один, и регистрирует его
/// \ref addMicrostrip.
void addMicrostrip2(QVector<Sample> *out);
void addMedia(QVector<Sample> *out);
void addLessons(QVector<Sample> *out);
const QVector<Baked> &bakedTable();

} // namespace tpl::samples
