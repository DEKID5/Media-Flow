#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

// Deep filesystem scan on a worker thread (mirrors Electron scanJwMedia roots).
class MediaScanWorker final : public QObject
{
    Q_OBJECT

public:
    explicit MediaScanWorker(QObject *parent = nullptr);

public slots:
    void scan(const QStringList &extraOrExclusiveRoots, const QString &languageFilter, bool includeDefaultJwPaths);

signals:
    void scanFinished(const QVariantList &assets);
    void scanFailed(const QString &message);

private:
    static QStringList defaultScanRoots();
    static void scanDir(const QString &dir, const QStringList &extensions, int depth, QVariantList *out);
};
