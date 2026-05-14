#include <QApplication>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "BroadcastController.h"
#include "MediaLibraryModel.h"
#include "MediaLibraryProxyModel.h"
#include "MeetingScheduleModel.h"
#include "SongManager.h"
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
    QApplication app(argc, argv);
    
    qRegisterMetaType<QCameraDevice>("QCameraDevice");
    QApplication::setOrganizationName(QStringLiteral("MediaFlow"));
    QApplication::setApplicationName(QStringLiteral("MediaFlow"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    qmlRegisterUncreatableType<MediaLibraryModel>("MediaFlow", 1, 0, "MediaLibraryModel",
                                                  QStringLiteral("Use MediaFlow.mediaLibrary"));

    BroadcastController controller(&engine);
    SongManager songManager;
    TimerController timerController;
    
    // Register the controller as a singleton instance
    qmlRegisterSingletonInstance("MediaFlow", 1, 0, "MediaFlowBackend", &controller);
    qmlRegisterSingletonInstance("MediaFlow", 1, 0, "JWSongManager", &songManager);
    qmlRegisterSingletonInstance("MediaFlow", 1, 0, "TimerBackend", &timerController);
    
    // Also keep context property as fallback
    engine.rootContext()->setContextProperty(QStringLiteral("MediaFlowBackend"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("JWSongManager"), &songManager);
    engine.rootContext()->setContextProperty(QStringLiteral("TimerBackend"), &timerController);

    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, &QCoreApplication::quit);

    engine.load(QUrl(QStringLiteral("qrc:/MediaFlow/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
