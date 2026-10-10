#include <QApplication>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include "BroadcastController.h"
#include "MediaLibraryModel.h"
#include "MeetingScheduleModel.h"
#include "TimerController.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <timeapi.h>
#endif

#include <QFile>
#include <QTextStream>
#include <QDateTime>

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QFile logFile("app_log.txt");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << msg << Qt::endl;
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageHandler);

#ifdef Q_OS_WIN
    // Windows' default system timer resolution (~15.6ms) is coarse enough
    // that losing foreground focus -- which can let Windows deprioritize
    // this process's scheduling -- is enough to starve the audio pipeline
    // and cause audible buffer-underrun stutter, even with no Zoom
    // broadcasting or other heavy work going on (confirmed: reported
    // stutter persisted while multitasking with a lightweight app). This is
    // a long-documented class of issue for Windows media apps; requesting a
    // 1ms resolution for the process's whole lifetime is the standard fix.
    // Paired with timeEndPeriod(1) just before returning, below.
    timeBeginPeriod(1);

    // Still wasn't enough on its own: stutter was reported specifically
    // while *scrolling* in another app -- the kind of burst of foreground
    // compositor/input activity that Windows' Process Power Throttling
    // (EcoQoS) is designed to react to by throttling CPU scheduling/
    // frequency for processes it judges unimportant, i.e. anything that
    // isn't currently foregrounded. MediaFlow still needs to keep decoding
    // and feeding audio smoothly in exactly that situation, so explicitly
    // opt this process out of execution-speed throttling regardless of
    // focus state -- the documented mechanism for "don't throttle me just
    // because I'm in the background" (the same one browsers/media apps use
    // to avoid audio glitches while backgrounded).
    {
        PROCESS_POWER_THROTTLING_STATE throttlingState = {};
        throttlingState.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        throttlingState.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        throttlingState.StateMask = 0; // 0 = not throttled, for every bit covered by ControlMask
        SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttlingState, sizeof(throttlingState));
    }
#endif

    // Must run before QApplication is constructed -- Qt locks in the RHI
    // backend (Direct3D11 by default on Windows) as part of QGuiApplication
    // construction, so calling this after `QApplication app(...)` is a
    // silent no-op that leaves the app on D3D11. VirtualCameraManager's
    // capture path uses QOpenGLContext/glReadPixels directly, which only
    // exists under the OpenGL RHI backend -- without this running early,
    // QOpenGLContext::currentContext() is always null during
    // afterRendering, so onAfterRendering() returns immediately and no
    // frame is ever captured or sent to the OBS Virtual Camera queue.
    //
    // This forces the whole app (every window, not just the Zoom capture
    // one -- Qt has no per-window RHI backend) onto OpenGL instead of
    // Windows' D3D11 default, which is the likely cause of Extended Feed
    // stutter/blank-frame flashes on focus loss (confirmed architecturally;
    // desktop OpenGL's swap-chain behavior on Windows is weaker across
    // focus changes than D3D11). Tried routing through ANGLE
    // (QT_OPENGL=angle) to get D3D11-backed behavior while keeping the
    // same OpenGL API surface glReadPixels needs -- Qt6 rejects that env
    // var outright ("no longer supported"), so that mitigation isn't
    // available here. A real fix needs either a backend-agnostic rewrite
    // of the capture path (QRhi texture readback instead of glReadPixels)
    // or moving Zoom capture into a separate process -- both larger,
    // riskier changes than this pass attempted.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    // Windows blocks drag-and-drop from a normal (non-elevated) Explorer into
    // an elevated process, so an admin-launched MediaFlow silently ignores
    // every file drop. Log it so that cause is diagnosable from app_log.txt.
    if (IsUserAnAdmin())
        qWarning() << "MediaFlow is running elevated (Administrator): drag-and-drop from Explorer will not work. Launch it normally.";
#endif

    qRegisterMetaType<QCameraDevice>("QCameraDevice");
    QApplication::setOrganizationName(QStringLiteral("MediaFlow"));
    QApplication::setApplicationName(QStringLiteral("MediaFlow"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    qmlRegisterUncreatableType<MediaLibraryModel>("MediaFlow", 1, 0, "MediaLibraryModel",
                                                  QStringLiteral("Use MediaFlow.mediaLibrary"));

    BroadcastController controller(&engine);
    TimerController timerController;

    // Register the controller as a singleton instance
    qmlRegisterSingletonInstance("MediaFlow", 1, 0, "MediaFlowBackend", &controller);
    qmlRegisterSingletonInstance("MediaFlow", 1, 0, "TimerBackend", &timerController);

    // Also keep context property as fallback
    engine.rootContext()->setContextProperty(QStringLiteral("MediaFlowBackend"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("TimerBackend"), &timerController);

    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, &QCoreApplication::quit);

    // The full-screen timer is its own dedicated window (opened/closed by
    // BroadcastController, positioned per its timerScreenIndex setting) so
    // it can target a different monitor than Extended Feed -- TimerController
    // doesn't own a QML engine/window itself, so this wiring lives here
    // rather than inside either class.
    QObject::connect(&timerController, &TimerController::fullScreenTimerChanged, &controller, [&controller, &timerController]() {
        controller.setTimerFullScreenActive(timerController.fullScreenTimer());
    });

    engine.load(QUrl(QStringLiteral("qrc:/MediaFlow/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    const int result = app.exec();

#ifdef Q_OS_WIN
    timeEndPeriod(1); // matches timeBeginPeriod(1) above
#endif

    return result;
}
