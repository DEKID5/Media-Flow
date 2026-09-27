#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief The MediaLibraryModel class (MediaSourceModel)
 * Holds all discovered or imported media assets for the broadcast suite.
 */
class MediaLibraryModel : public QAbstractListModel
{
    Q_OBJECT

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
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendFromVariantList(const QVariantList &items);
    void clear();

    /**
     * @brief Returns a QVariantMap for a media row by its ID.
     * Exposed to QML so linked media chips can resolve metadata.
     */
    Q_INVOKABLE QVariantMap getRowById(const QString &id) const;
    Q_INVOKABLE QString idOfPath(const QString &path) const;
    Q_INVOKABLE QVariantList toVariantList() const;

    void updateThumbnail(const QString &id, const QString &thumbnailData);
    void updateName(const QString &id, const QString &newName);

    /**
     * @brief Checks if a file path is already indexed.
     */
    bool containsPath(const QString &path) const;

    /**
     * @brief Returns a list of unique categories.
     */
    Q_INVOKABLE QStringList categories() const;

    /**
     * @brief Batch updates categories.
     */
    Q_INVOKABLE void renameCategory(const QString &oldName, const QString &newName);

    /**
     * @brief Removes a single media asset by ID.
     */
    Q_INVOKABLE void removeMedia(const QString &id);


private:
    QVector<Row> m_rows;
};
