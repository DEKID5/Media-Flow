#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>

/**
 * @brief The TimerController class manages the meeting timer with high precision.
 * Features: MM:SS formatting, Overtime handling (negative count-up), and Staging.
 */
class TimerController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(TimerState state READ state NOTIFY stateChanged)
    Q_PROPERTY(int targetDurationSeconds READ targetDurationSeconds WRITE setTargetDurationSeconds NOTIFY targetDurationChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY timeChanged)
    Q_PROPERTY(QString displayTime READ displayTime NOTIFY timeChanged)
    Q_PROPERTY(bool isStaged READ isStaged WRITE setIsStaged NOTIFY stagingChanged)

public:
    enum TimerState {
        Idle,
        Running,
        Paused,
        Overtime
    };
    Q_ENUM(TimerState)

    explicit TimerController(QObject *parent = nullptr);

    // Getters
    TimerState state() const { return m_state; }
    int targetDurationSeconds() const { return m_targetDuration; }
    int elapsedSeconds() const { return m_elapsedSeconds; }
    QString displayTime() const;
    bool isStaged() const { return m_isStaged; }

    // Setters
    void setTargetDurationSeconds(int seconds);
    void setIsStaged(bool staged);

    // Invokables
    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void stage() { setIsStaged(!m_isStaged); }
    Q_INVOKABLE void adjustDuration(int deltaMinutes);

signals:
    void stateChanged();
    void targetDurationChanged();
    void timeChanged();
    void stagingChanged();
    void stagedSignal(); // For Audience View to force display

private slots:
    void onTick();

private:
    void updateState();
    void calculateElapsed();

    TimerState m_state = Idle;
    int m_targetDuration = 300; // Default 5 mins
    int m_elapsedSeconds = 0;
    bool m_isStaged = false;

    QTimer *m_tickTimer;
    QElapsedTimer m_elapsedTimer;
    qint64 m_accumulatedMs = 0;
};
