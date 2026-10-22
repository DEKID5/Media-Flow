#include "MediaLibraryModel.h"
#include <QDebug>
#include <QUrl>
#include <QRegularExpression>

MediaLibraryModel::MediaLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MediaLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_rows.size();
}

const MediaLibraryModel::Row &MediaLibraryModel::rowAt(int index) const
{
    static const Row s_emptyRow{};
    if (index >= 0 && index < m_rows.size())
        return m_rows.at(index);
    return s_emptyRow;
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
    static const QRegularExpression langRegex(QStringLiteral("_[A-Z]{1,3}_"));

    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + list.size() - 1);
    m_rows.reserve(m_rows.size() + list.size());
    m_idToIndex.reserve(m_idToIndex.size() + list.size());
    m_pathToId.reserve(m_pathToId.size() + list.size());

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

        if (r.type == QLatin1String("video") || r.type == QLatin1String("audio")) {
            const qsizetype fileNameStart = qMax(r.path.lastIndexOf(QLatin1Char('/')), r.path.lastIndexOf(QLatin1Char('\\'))) + 1;
            const QString fileName = r.path.mid(fileNameStart);
            const QString haystack = fileName.isEmpty() ? r.name : fileName;
            auto match = langRegex.match(haystack);
            if (!match.hasMatch()) match = langRegex.match(r.path);
            if (match.hasMatch()) r.languageMarker = match.captured(0);
        }

        const int newIdx = m_rows.size();
        m_idToIndex.insert(r.id, newIdx);
        m_pathToId.insert(r.path, r.id);
        m_rows.append(r);
    }
    endInsertRows();
    emit categoriesChanged();
}

void MediaLibraryModel::updateThumbnail(const QString &id, const QString &path)
{
    const auto it = m_idToIndex.constFind(id);
    if (it == m_idToIndex.constEnd() || it.value() < 0 || it.value() >= m_rows.size())
        return;
    const int i = it.value();
    if (m_rows[i].thumbnail == path)
        return;
    m_rows[i].thumbnail = path;
    ++m_thumbnailRevision;
    emit dataChanged(index(i), index(i), {ThumbnailRole});
    emit thumbnailRevisionChanged();
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
    const auto it = m_idToIndex.constFind(id);
    if (it == m_idToIndex.constEnd() || it.value() < 0 || it.value() >= m_rows.size())
        return {};
    const auto &r = m_rows.at(it.value());
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

QVariantList MediaLibraryModel::toVariantList() const
{
    QVariantList items;
    items.reserve(m_rows.size());
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
    bool changed = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].category == oldName) {
            m_rows[i].category = newName;
            emit dataChanged(index(i), index(i), {CategoryRole});
            changed = true;
        }
    }
    if (changed)
        emit categoriesChanged();
}

void MediaLibraryModel::removeMedia(const QString &id)
{
    const auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) return;
    const int idx = it.value();
    beginRemoveRows(QModelIndex(), idx, idx);
    m_pathToId.remove(m_rows[idx].path);
    m_rows.removeAt(idx);
    m_idToIndex.clear();
    for (int i = 0; i < m_rows.size(); ++i) {
        m_idToIndex.insert(m_rows[i].id, i);
    }
    endRemoveRows();
    emit categoriesChanged();
}

void MediaLibraryModel::clear()
{
    if (m_rows.isEmpty()) return;
    beginResetModel();
    m_rows.clear();
    m_idToIndex.clear();
    m_pathToId.clear();
    endResetModel();
    emit categoriesChanged();
}

bool MediaLibraryModel::containsPath(const QString &path) const
{
    return m_pathToId.contains(path);
}

QString MediaLibraryModel::idOfPath(const QString &path) const
{
    return m_pathToId.value(path);
}

void MediaLibraryModel::updateName(const QString &id, const QString &newName)
{
    if (newName.isEmpty()) return;
    const auto it = m_idToIndex.constFind(id);
    if (it == m_idToIndex.constEnd() || it.value() < 0 || it.value() >= m_rows.size())
        return;
    const int i = it.value();
    m_rows[i].name = newName;
    emit dataChanged(index(i), index(i), {NameRole});
}

void MediaLibraryModel::markImported(const QString &id, const QString &category)
{
    const auto it = m_idToIndex.constFind(id);
    if (it == m_idToIndex.constEnd() || it.value() < 0 || it.value() >= m_rows.size())
        return;
    const int i = it.value();
    if (m_rows[i].imported && m_rows[i].category == category)
        return;
    m_rows[i].imported = true;
    const bool categoryChanged = m_rows[i].category != category;
    m_rows[i].category = category;
    emit dataChanged(index(i), index(i), {ImportedRole, CategoryRole});
    if (categoryChanged)
        emit categoriesChanged();
}
