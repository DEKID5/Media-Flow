#include "JwLibraryWorker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

QStringList JwLibraryWorker::candidateDatabasePaths()
{
    QStringList out;
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    if (local.isEmpty())
        return out;

    const QStringList packages = {
        QStringLiteral("WatchtowerBibleandTractSo.JWLibrary_5z594pnt97tep"),
        QStringLiteral("WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e"),
    };

    for (const QString &pkg : packages) {
        const QString base = QDir(local).filePath(QStringLiteral("Packages/%1/LocalState").arg(pkg));
        out << QDir(base).filePath(QStringLiteral("userData.db"));
        out << QDir(base).filePath(QStringLiteral("Data/userData.db"));
    }
    return out;
}

JwLibraryWorker::JwLibraryWorker(QObject *parent)
    : QObject(parent)
{
}

void JwLibraryWorker::findDatabase()
{
    for (const QString &path : candidateDatabasePaths()) {
        if (QFileInfo::exists(path)) {
            emit databaseFound(QFileInfo(path).absoluteFilePath());
            return;
        }
    }
    emit databaseNotFound();
}

void JwLibraryWorker::inspectDatabase(const QString &sourcePath)
{
    if (sourcePath.isEmpty()) {
        emit inspectionFailed(tr("No database path."));
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString dest = QDir(tempDir).filePath(
        QStringLiteral("jw_userData_copy_%1.db").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    if (QFile::exists(dest))
        QFile::remove(dest);

    if (!QFile::copy(sourcePath, dest)) {
        emit inspectionFailed(tr("Could not copy database to temp (file may be locked)."));
        return;
    }

    const QString conn = QStringLiteral("jw_inspect_%1").arg(
        reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(dest);
        if (!db.open()) {
            QSqlDatabase::removeDatabase(conn);
            emit inspectionFailed(db.lastError().text());
            return;
        }

        QStringList tables;
        {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"))) {
                while (q.next())
                    tables << q.value(0).toString();
            }
        }

        QString summary = tr("Temp copy: %1\nTables (%2):\n").arg(dest).arg(tables.size());
        const int cap = 40;
        for (int i = 0; i < tables.size() && i < cap; ++i)
            summary += tables.at(i) + QLatin1Char('\n');
        if (tables.size() > cap)
            summary += tr("… (%1 more)").arg(tables.size() - cap);

        db.close();
        QSqlDatabase::removeDatabase(conn);
        emit inspectionFinished(summary);
    }
}
