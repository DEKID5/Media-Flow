#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariantList>

/**
 * @brief User-created, persisted collections of media asset ids ("pin
 * folders"). Independent of the JW Library category system -- these are
 * the operator's own curated groupings, filled by dragging media in from
 * the library grid or dropping files in from Windows Explorer.
 */
class PinnedFolderModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        MediaIdsRole,
        CountRole
    };

    struct Folder {
        QString id;
        QString name;
        QStringList mediaIds;
    };

    explicit PinnedFolderModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString createFolder(const QString &name);
    Q_INVOKABLE void renameFolder(const QString &folderId, const QString &newName);
    Q_INVOKABLE void deleteFolder(const QString &folderId);
    Q_INVOKABLE void addMedia(const QString &folderId, const QString &mediaId);
    Q_INVOKABLE void removeMedia(const QString &folderId, const QString &mediaId);
    Q_INVOKABLE QStringList mediaIdsForFolder(const QString &folderId) const;
    Q_INVOKABLE QString nameForFolder(const QString &folderId) const;

    QVariantList getFullState() const;
    void setFullState(const QVariantList &data);

private:
    int rowOfId(const QString &folderId) const;

    QVector<Folder> m_folders;
};
