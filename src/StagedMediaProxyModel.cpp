#include "StagedMediaProxyModel.h"
#include "MediaLibraryModel.h"

StagedMediaProxyModel::StagedMediaProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
}

void StagedMediaProxyModel::setSelectedSegmentId(const QString &id)
{
    if (m_selectedSegmentId != id) {
        m_selectedSegmentId = id;
        invalidateFilter();
        emit selectedSegmentIdChanged();
    }
}

void StagedMediaProxyModel::setStagedIds(const QStringList &ids)
{
    if (m_stagedIds != ids) {
        m_stagedIds = ids;
        invalidateFilter();
        emit stagedIdsChanged();
    }
}

void StagedMediaProxyModel::setFilterType(const QString &t)
{
    if (m_filterType != t) {
        m_filterType = t;
        invalidateFilter();
        emit filterChanged();
    }
}

void StagedMediaProxyModel::setCategoryFilter(const QString &c)
{
    if (m_categoryFilter != c) {
        m_categoryFilter = c;
        invalidateFilter();
        emit filterChanged();
    }
}

bool StagedMediaProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (m_filterType == "all") return true;

    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    
    if (m_filterType == "segment") {
        if (m_selectedSegmentId.isEmpty()) return false;
        QString assetUuid = sourceModel()->data(idx, MediaLibraryModel::IdRole).toString();
        return m_stagedIds.contains(assetUuid);
    }

    if (m_filterType == "imported") {
        return sourceModel()->data(idx, MediaLibraryModel::ImportedRole).toBool();
    }

    if (m_filterType == "videos") {
        return sourceModel()->data(idx, MediaLibraryModel::TypeRole).toString() == "video";
    }
    if (m_filterType == "images") {
        return sourceModel()->data(idx, MediaLibraryModel::TypeRole).toString() == "image";
    }
    if (m_filterType == "audio") {
        return sourceModel()->data(idx, MediaLibraryModel::TypeRole).toString() == "audio";
    }

    if (m_filterType == "category") {
        if (m_categoryFilter.isEmpty()) return true;
        return sourceModel()->data(idx, MediaLibraryModel::CategoryRole).toString() == m_categoryFilter;
    }

    return true;
}
