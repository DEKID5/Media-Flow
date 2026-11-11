#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

// Holds all discovered and imported media for the broadcast suite.
class MediaLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(quint64 thumbnailRevision READ thumbnailRevision NOTIFY thumbnailRevisionChanged)

public:
    struct Row {
        QString id;
        QString name;
        QString type;    // "video", "image", "audio", "input"
        QString path;
        QString thumbnail;
        QString category; // e.g. "General", "Songs", "Images"
        bool staged = false;
        bool imported = false;
        long long size = 0;
        QString languageMarker; // e.g. "_E_", "_S_", or empty
    };

    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        PathRole,
        ThumbnailRole,
        StagedRole,
        ImportedRole,
        CategoryRole
    };

    explicit MediaLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    const Row &rowAt(int index) const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendFromVariantList(const QVariantList &items);
    void clear();

    // Return row metadata by ID for QML.
    Q_INVOKABLE QVariantMap getRowById(const QString &id) const;
    Q_INVOKABLE QString idOfPath(const QString &path) const;
    Q_INVOKABLE QVariantList toVariantList() const;

    void updateThumbnail(const QString &id, const QString &thumbnailData);
    void updateName(const QString &id, const QString &newName);
    void markImported(const QString &id, const QString &category);

    // Check whether a media path is already indexed.
    bool containsPath(const QString &path) const;

    // Return the list of unique media categories.
    QStringList categories() const;
    quint64 thumbnailRevision() const { return m_thumbnailRevision; }

    // Rename a category across all rows in one pass.
    Q_INVOKABLE void renameCategory(const QString &oldName, const QString &newName);

    // Remove a single media asset by ID.
    Q_INVOKABLE void removeMedia(const QString &id);

signals:
    void categoriesChanged();
    void thumbnailRevisionChanged();

private:
    QVector<Row> m_rows;
    QHash<QString, int> m_idToIndex;
    QHash<QString, QString> m_pathToId;
    quint64 m_thumbnailRevision = 0;
};
