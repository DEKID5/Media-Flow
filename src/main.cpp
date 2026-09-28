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

    // Must run before QApplication is constructed -- Qt locks in the RHI
    // backend (Direct3D11 by default on Windows) as part of QGuiApplication
    // construction, so calling this after `QApplication app(...)` is a
    // silent no-op that leaves the app on D3D11. VirtualCameraManager's
    // capture path uses QOpenGLContext/glReadPixels directly, which only
    // exists under the OpenGL RHI backend -- without this running early,
    // QOpenGLContext::currentContext() is always null during
    // afterRendering, so onAfterRendering() returns immediately and no
    // frame is ever captured or sent to the OBS Virtual Camera queue.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QApplication app(argc, argv);

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

    return app.exec();
}
