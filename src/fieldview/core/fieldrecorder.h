// Запись серии тактов поля во время расчёта (решение M26).
//
// Ядро с FIELDS переписывает ОДИН файл <имя>.ex каждые полтакта (TmcRTHRectNode.cpp,
// PutField); Windows-FieldView следил за ним таймером 50 мс и перерисовывал — это и
// была «анимация». Здесь то же наблюдение, но такты дописываются копией байт в байт в
// <имя>.ex.frames рядом с .ex, чтобы потом проигрывать их вперёд и назад: тот же
// формат кадра, но значения в одинарной точности (Accuracy 4, вдвое меньше; экран
// рисует поле во float). Ядро и формат его файлов не меняются.
//
// Плотность. Чтобы волна на экране шла плавно, кадры берутся равномерно по тактам и
// часто: шаг = тактов в шаге расчёта / число кадров (не больше maxFrames и не больше,
// чем помещается в бюджет). Опрос — в отдельном потоке каждые interval мс. Сначала
// читается только заголовок кадра (номер такта); целиком кадр читается, лишь когда
// он нужен.
//
// Целостность кадра:
//   * Windows — файл открывается без разрешения записи другим (FILE_SHARE_READ):
//     пока он открыт, ядро не может начать новую запись (fopen "wb" повторяется
//     через 10 мс — так устроено в ядре), значит прочитан законченный кадр;
//   * Android/iOS/Linux — ядро пишет кадр во временный файл и подменяет им файл поля
//     переименованием (compat tmc_fopen, ISSUES П-10) — прочитан всегда цельный кадр;
//     размер ровно равен «заголовок + данные».
// Кадр с тем же номером такта, что предыдущий, не пишется. При превышении бюджета
// запись прекращается (о чём сообщает status()).
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include <atomic>
#include <thread>

namespace fieldview {

class FieldRecorder : public QObject
{
    Q_OBJECT
public:
    explicit FieldRecorder(QObject *parent = nullptr);
    ~FieldRecorder() override;

    /// Наблюдать за всеми *.ex в папке расчёта (в своём потоке). Старые *.ex.frames
    /// там удаляются.
    void start(const QString &workDir);
    /// Остановить поток и сделать последний опрос (дописать финальный такт).
    void stop();
    bool isActive() const { return m_thread.joinable(); }

    /// Пауза между опросами, мс (по умолчанию 2).
    void setInterval(int ms) { m_intervalMs = ms; }
    /// Наибольший объём всех серий, байт (по умолчанию 256 МБ; приложение — 160 МБ).
    void setBudget(qint64 bytes) { m_budget = bytes; }
    /// Не писать такт, если от прошлого записанного прошло меньше step тактов.
    void setMinTactStep(int step) { m_minStep = step; }
    /// Тактов в текущем шаге расчёта (из хода ядра, «nTmax») — для равномерного шага кадров.
    void setExpectedTicks(int ticks) { m_expectedTicks = ticks; }
    /// Наибольшее число кадров на шаг расчёта (по умолчанию 800).
    void setMaxFrames(int frames) { m_maxFrames = frames; }
    /// Пока число тактов не известно (setExpectedTicks), брать только первый кадр шага —
    /// иначе начало расчёта записывалось бы каждым тактом.
    void setRequireTicks(bool on) { m_requireTicks = on; }

    // Читать после stop() (поток к этому времени остановлен).
    int framesWritten() const { return m_frames; }
    qint64 bytesWritten() const { return m_bytes; }
    QString status() const { return m_status; }

    /// Один опрос (вызывается из потока записи; открыт для проверок).
    void poll();

Q_SIGNALS:
    void frameRecorded(const QString &exFile, int nT);

private:
    struct Track
    {
        int lastNT = -1;
    };
    bool captureOne(const QString &exPath, Track *t);
    int stepFor(qint64 frameBytes) const;

    QString m_dir;
    std::thread m_thread;
    std::atomic<bool> m_stop{ false };
    std::atomic<int> m_intervalMs{ 2 };
    std::atomic<int> m_minStep{ 0 };
    std::atomic<int> m_expectedTicks{ 0 };
    std::atomic<int> m_maxFrames{ 800 };
    std::atomic<bool> m_requireTicks{ false };
    QHash<QString, Track> m_tracks;
    qint64 m_budget = 256LL * 1024 * 1024;
    int m_frames = 0;
    qint64 m_bytes = 0;
    QString m_status;
};

} // namespace fieldview
