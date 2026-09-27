#include "MediaLibraryModel.h"
#include <QDebug>
#include <QUrl>

MediaLibraryModel::MediaLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MediaLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_rows.size();
}

QVariant MediaLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return {};
    const auto &r = m_rows[index.row()];
    switch (role) {
        case IdRole: return r.id;
        case NameRole: return r.name;
        case TypeRole: return r.type;
        case PathRole: return r.path;
        case ThumbnailRole: return r.thumbnail;
        case StagedRole: return r.staged;
        case ImportedRole: return r.imported;
        case CategoryRole: return r.category;
    }
    return {};
}

QHash<int, QByteArray> MediaLibraryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[TypeRole] = "type";
    roles[PathRole] = "absolutePath";
    roles[ThumbnailRole] = "thumbnailPath";
    roles[StagedRole] = "isStaged";
    roles[ImportedRole] = "isImported";
    roles[CategoryRole] = "category";
    return roles;
}

void MediaLibraryModel::appendFromVariantList(const QVariantList &list)
{
    if (list.isEmpty()) return;
    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + list.size() - 1);
    for (const QVariant &v : list) {
        QVariantMap m = v.toMap();
        Row r;
        r.id = m["id"].toString();
        r.name = m["name"].toString();
        r.type = m["type"].toString();
        r.path = m["absolutePath"].toString();
        r.thumbnail = m["thumbnailPath"].toString();
        r.staged = m["isStaged"].toBool();
        r.imported = m["isImported"].toBool();
        r.category = m["category"].toString();
        m_rows.append(r);
    }
    endInsertRows();
}

void MediaLibraryModel::updateThumbnail(const QString &id, const QString &path)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].id == id) {
            m_rows[i].thumbnail = path;
            emit dataChanged(index(i), index(i), {ThumbnailRole});
            return;
        }
    }
}

QStringList MediaLibraryModel::categories() const
{
    QStringList cats;
    for (const auto &r : m_rows) {
        if (!cats.contains(r.category)) cats.append(r.category);
    }
    if (cats.isEmpty()) cats << "General";
    return cats;
}

QVariantMap MediaLibraryModel::getRowById(const QString &id) const
{
    for (const auto &r : m_rows) {
        if (r.id == id) {
            QVariantMap m;
            m.insert("id", r.id);
            m.insert("name", r.name);
            m.insert("type", r.type);
            m.insert("absolutePath", r.path);
            m.insert("thumbnailPath", r.thumbnail);
            m.insert("isStaged", r.staged);
            m.insert("isImported", r.imported);
            m.insert("category", r.category);
            return m;
        }
    }
    return {};
}

QVariantList MediaLibraryModel::toVariantList() const
{
    QVariantList items;
    for (const auto &r : m_rows) {
        QVariantMap m;
        m.insert("id", r.id);
        m.insert("name", r.name);
        m.insert("type", r.type);
        m.insert("absolutePath", r.path);
        m.insert("thumbnailPath", r.thumbnail);
        m.insert("isStaged", r.staged);
        m.insert("isImported", r.imported);
        m.insert("category", r.category);
        items.append(m);
    }
    return items;
}

void MediaLibraryModel::renameCategory(const QString &oldName, const QString &newName)
{
    if (oldName == newName) return;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].category == oldName) {
            m_rows[i].category = newName;
            emit dataChanged(index(i), index(i), {CategoryRole});
        }
    }
}

void MediaLibraryModel::removeMedia(const QString &id)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            m_rows.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

void MediaLibraryModel::clear()
{
    if (m_rows.isEmpty()) return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

bool MediaLibraryModel::containsPath(const QString &path) const
{
    return !idOfPath(path).isEmpty();
}

QString MediaLibraryModel::idOfPath(const QString &path) const
{
    for (const auto &r : m_rows) {
        if (r.path == path) return r.id;
    }
    return "";
}

void MediaLibraryModel::updateName(const QString &id, const QString &newName)
{
    if (newName.isEmpty()) return;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].id == id) {
            m_rows[i].name = newName;
            emit dataChanged(index(i), index(i), {NameRole});
            return;
        }
    }
}
