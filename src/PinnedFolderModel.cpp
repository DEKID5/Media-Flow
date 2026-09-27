#include "PinnedFolderModel.h"

#include <QUuid>
#include <QVariantMap>

PinnedFolderModel::PinnedFolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PinnedFolderModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_folders.size();
}

QVariant PinnedFolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_folders.size()) return {};
    const Folder &f = m_folders[index.row()];
    switch (role) {
        case IdRole: return f.id;
        case NameRole: return f.name;
        case MediaIdsRole: return f.mediaIds;
        case CountRole: return f.mediaIds.size();
    }
    return {};
}

QHash<int, QByteArray> PinnedFolderModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {MediaIdsRole, "mediaIds"},
        {CountRole, "count"}
    };
}

int PinnedFolderModel::rowOfId(const QString &folderId) const
{
    for (int i = 0; i < m_folders.size(); ++i) {
        if (m_folders[i].id == folderId) return i;
    }
    return -1;
}

QString PinnedFolderModel::createFolder(const QString &name)
{
    const QString trimmed = name.trimmed();
    Folder f;
    f.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    f.name = trimmed.isEmpty() ? QStringLiteral("New Pin") : trimmed;

    beginInsertRows(QModelIndex(), m_folders.size(), m_folders.size());
    m_folders.append(f);
    endInsertRows();
    return f.id;
}

void PinnedFolderModel::renameFolder(const QString &folderId, const QString &newName)
{
    const int row = rowOfId(folderId);
    if (row == -1 || newName.trimmed().isEmpty()) return;
    m_folders[row].name = newName.trimmed();
    emit dataChanged(index(row), index(row), {NameRole});
}

void PinnedFolderModel::deleteFolder(const QString &folderId)
{
    const int row = rowOfId(folderId);
    if (row == -1) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_folders.removeAt(row);
    endRemoveRows();
}

void PinnedFolderModel::addMedia(const QString &folderId, const QString &mediaId)
{
    if (mediaId.isEmpty()) return;
    const int row = rowOfId(folderId);
    if (row == -1) return;
    if (!m_folders[row].mediaIds.contains(mediaId)) {
        m_folders[row].mediaIds.append(mediaId);
        emit dataChanged(index(row), index(row), {MediaIdsRole, CountRole});
    }
}

void PinnedFolderModel::removeMedia(const QString &folderId, const QString &mediaId)
{
    const int row = rowOfId(folderId);
    if (row == -1) return;
    if (m_folders[row].mediaIds.removeAll(mediaId) > 0)
        emit dataChanged(index(row), index(row), {MediaIdsRole, CountRole});
}

QStringList PinnedFolderModel::mediaIdsForFolder(const QString &folderId) const
{
    const int row = rowOfId(folderId);
    if (row == -1) return {};
    return m_folders[row].mediaIds;
}

QString PinnedFolderModel::nameForFolder(const QString &folderId) const
{
    const int row = rowOfId(folderId);
    if (row == -1) return {};
    return m_folders[row].name;
}

QVariantList PinnedFolderModel::getFullState() const
{
    QVariantList list;
    for (const Folder &f : m_folders) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), f.id);
        m.insert(QStringLiteral("name"), f.name);
        m.insert(QStringLiteral("mediaIds"), f.mediaIds);
        list.append(m);
    }
    return list;
}

void PinnedFolderModel::setFullState(const QVariantList &data)
{
    beginResetModel();
    m_folders.clear();
    for (const QVariant &v : data) {
        const QVariantMap m = v.toMap();
        Folder f;
        f.id = m.value(QStringLiteral("id")).toString();
        f.name = m.value(QStringLiteral("name")).toString();
        f.mediaIds = m.value(QStringLiteral("mediaIds")).toStringList();
        if (!f.id.isEmpty())
            m_folders.append(f);
    }
    endResetModel();
}
