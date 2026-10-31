/// @file touchgesture.h
/// @brief Разбор касаний для графиков и поля: один палец, щипок, касание, двойное касание.
///
/// Только в версии для Android: настольные версии управляются мышью. Обработчики QML здесь не
/// годятся: при начале щипка они выдают первое смещение раньше, чем можно запомнить начальную
/// точку, и вид прыгает, а обработчик одного пальца успевает повернуть сцену. Этот разбор держит
/// одно состояние на всё касание: второй палец переводит в щипок без скачка, а после щипка
/// оставшийся палец ничего не двигает, пока не будут отпущены все пальцы.
/// @see https://www.tamic.ru/docs/platforms/android.html#gestures
#pragma once

#include <QElapsedTimer>
#include <QEvent>
#include <QLineF>
#include <QPointF>
#include <QTouchEvent>
#include <QVector>

#include <functional>

/// @brief Состояние одного касания и обратные вызовы элемента, который его получает.
///
/// Элемент задаёт нужные функции и передаёт в `process` каждое `QTouchEvent`. Координаты — в
/// системе элемента. Потоки: только поток интерфейса.
class TouchGesture
{
public:
    /// Перемещение одного пальца: смещение с прошлого события, пиксели.
    std::function<void(const QPointF &delta)> onDrag;
    /// Щипок: множитель масштаба, центр между пальцами, смещение центра, пиксели.
    std::function<void(double factor, const QPointF &center, const QPointF &delta)> onPinch;
    /// Короткое касание одним пальцем.
    std::function<void(const QPointF &pos)> onTap;
    /// Двойное касание.
    std::function<void(const QPointF &pos)> onDoubleTap;

    /// @brief Разбирает событие касания и вызывает обратные вызовы.
    /// @param[in] e  событие; принимается всегда.
    void process(QTouchEvent *e)
    {
        e->accept();
        if (e->type() == QEvent::TouchCancel) {
            reset();
            return;
        }
        QVector<QPointF> down;
        for (const QEventPoint &p : e->points()) {
            if (p.state() != QEventPoint::Released)
                down << p.position();
        }
        if (down.size() >= 2)
            twoFingers(down.at(0), down.at(1));
        else if (down.size() == 1)
            oneFinger(down.at(0));
        else
            released();
    }

    /// @brief Забывает начатое касание.
    void reset()
    {
        m_state = State::Idle;
        m_hadPinch = false;
    }

private:
    /// Сдвиг пальца, после которого касание становится протяжкой, пиксели.
    static constexpr double kSlop = 10.0;
    /// Наибольшая пауза между касаниями двойного касания, мс.
    static constexpr int kDoubleMs = 300;
    /// Наибольшее расстояние между касаниями двойного касания, пиксели.
    static constexpr double kDoubleDist = 40.0;

    enum class State { Idle, Pressed, Dragging, Pinching, AfterPinch };

    void twoFingers(const QPointF &a, const QPointF &b)
    {
        const QPointF center = (a + b) / 2.0;
        const double dist = QLineF(a, b).length();
        if (m_state != State::Pinching) {
            // Начальные значения запоминаются здесь, первое событие ничего не двигает.
            m_state = State::Pinching;
            m_hadPinch = true;
        } else if (m_dist > 1.0 && dist > 1.0 && onPinch) {
            onPinch(dist / m_dist, center, center - m_center);
        }
        m_dist = dist;
        m_center = center;
    }

    void oneFinger(const QPointF &p)
    {
        switch (m_state) {
        case State::Idle:
            m_state = State::Pressed;
            m_press = m_last = p;
            break;
        case State::Pressed:
            if (QLineF(p, m_press).length() > kSlop) {
                m_state = State::Dragging;
                m_last = p;
            }
            break;
        case State::Dragging:
            if (onDrag)
                onDrag(p - m_last);
            m_last = p;
            break;
        case State::Pinching:
        case State::AfterPinch:
            m_state = State::AfterPinch;
            break;
        }
    }

    void released()
    {
        if (m_state == State::Pressed && !m_hadPinch) {
            const bool second = m_lastTap.isValid() && m_lastTap.elapsed() < kDoubleMs
                                && QLineF(m_press, m_lastTapPos).length() < kDoubleDist;
            if (second) {
                m_lastTap.invalidate();
                if (onDoubleTap)
                    onDoubleTap(m_press);
            } else {
                m_lastTap.start();
                m_lastTapPos = m_press;
                if (onTap)
                    onTap(m_press);
            }
        }
        reset();
    }

    State m_state = State::Idle;
    bool m_hadPinch = false;
    QPointF m_press;
    QPointF m_last;
    QPointF m_center;
    double m_dist = 0.0;
    QElapsedTimer m_lastTap;
    QPointF m_lastTapPos;
};
