#include "TimerController.h"
#include <QDebug>

TimerController::TimerController(QObject *parent)
    : QObject(parent)
    , m_tickTimer(new QTimer(this))
{
    m_tickTimer->setInterval(100);
    connect(m_tickTimer, &QTimer::timeout, this, &TimerController::onTick);
}

QString TimerController::displayTime() const
{
    int totalRemaining = m_targetDuration - m_elapsedSeconds;
    bool negative = totalRemaining < 0;
    int absSeconds = std::abs(totalRemaining);
    
    int m = absSeconds / 60;
    int s = absSeconds % 60;
    
    QString sign = negative ? "-" : "";
    return QString("%1%2:%3")
        .arg(sign)
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

void TimerController::setTargetDurationSeconds(int seconds)
{
    if (m_targetDuration != seconds) {
        m_targetDuration = std::max(0, seconds);
        emit targetDurationChanged();
        emit timeChanged();
        updateState();
    }
}

void TimerController::setIsStaged(bool staged)
{
    if (m_isStaged != staged) {
        m_isStaged = staged;
        emit stagingChanged();
        if (m_isStaged) emit stagedSignal();
    }
}

void TimerController::start()
{
    if (m_state != Running) {
        m_elapsedTimer.start();
        m_tickTimer->start();
        m_state = Running;
        updateState();
        emit stateChanged();
    }
}

void TimerController::pause()
{
    if (m_state == Running || m_state == Overtime) {
        m_accumulatedMs += m_elapsedTimer.elapsed();
        m_tickTimer->stop();
        m_state = Paused;
        emit stateChanged();
    }
}

void TimerController::reset()
{
    m_tickTimer->stop();
    m_accumulatedMs = 0;
    m_elapsedSeconds = 0;
    m_state = Idle;
    emit stateChanged();
    emit timeChanged();
}

void TimerController::adjustDuration(int deltaMinutes)
{
    setTargetDurationSeconds(m_targetDuration + (deltaMinutes * 60));
}

void TimerController::onTick()
{
    calculateElapsed();
    updateState();
}

void TimerController::calculateElapsed()
{
    qint64 currentMs = m_accumulatedMs;
    if (m_tickTimer->isActive()) {
        currentMs += m_elapsedTimer.elapsed();
    }
    
    int newElapsed = static_cast<int>(currentMs / 1000);
    if (newElapsed != m_elapsedSeconds) {
        m_elapsedSeconds = newElapsed;
        emit timeChanged();
    }
}

void TimerController::updateState()
{
    if (m_state == Idle || m_state == Paused) return;
    
    TimerState newState = (m_elapsedSeconds > m_targetDuration) ? Overtime : Running;
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged();
    }
}
