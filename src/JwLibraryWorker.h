#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Runs on a dedicated QThread: locate JW Library SQLite, copy to temp, inspect with QSqlDatabase.
class JwLibraryWorker final : public QObject
{
    Q_OBJECT

public:
    explicit JwLibraryWorker(QObject *parent = nullptr);

public slots:
    void findDatabase();
    void inspectDatabase(const QString &sourcePath);

signals:
    void databaseFound(const QString &absolutePath);
    void databaseNotFound();
    void inspectionFinished(const QString &summaryText);
    void inspectionFailed(const QString &message);

private:
    static QStringList candidateDatabasePaths();
};
