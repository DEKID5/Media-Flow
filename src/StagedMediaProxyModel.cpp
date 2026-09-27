#include "StagedMediaProxyModel.h"
#include "MediaLibraryModel.h"
#include "SongSearchUtils.h"

#include <QFileInfo>
#include <QRegularExpression>

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

void StagedMediaProxyModel::setLanguageCode(const QString &code)
{
    const QString normalized = SongSearchUtils::normalizeLanguageCode(code);
    if (m_languageCode != normalized) {
        m_languageCode = normalized;
        invalidateFilter();
        emit languageCodeChanged();
        emit filterChanged();
    }
}

bool StagedMediaProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    const QString type = sourceModel()->data(idx, MediaLibraryModel::TypeRole).toString();
    const QString name = sourceModel()->data(idx, MediaLibraryModel::NameRole).toString();
    const QString path = sourceModel()->data(idx, MediaLibraryModel::PathRole).toString();
    const QString fileName = QFileInfo(path).fileName();
    const QString haystack = fileName.isEmpty() ? name : fileName;

    if ((type == QStringLiteral("video") || type == QStringLiteral("audio"))) {
        static const QRegularExpression languageMarker(QStringLiteral("_[A-Z]{1,3}_"));
        if (languageMarker.match(haystack).hasMatch() || languageMarker.match(path).hasMatch()) {
            const QString required = QStringLiteral("_%1_").arg(m_languageCode);
            if (!haystack.contains(required, Qt::CaseInsensitive) && !path.contains(required, Qt::CaseInsensitive))
                return false;
        }
    }

    if (m_filterType == "all") return true;
    
    if (m_filterType == "segment") {
        if (m_selectedSegmentId.isEmpty()) return false;
        QString assetUuid = sourceModel()->data(idx, MediaLibraryModel::IdRole).toString();
        return m_stagedIds.contains(assetUuid);
    }

    if (m_filterType == "imported") {
        return sourceModel()->data(idx, MediaLibraryModel::ImportedRole).toBool();
    }

    if (m_filterType == "videos") {
        return type == "video";
    }
    if (m_filterType == "images") {
        return type == "image";
    }
    if (m_filterType == "audio") {
        return type == "audio";
    }

    if (m_filterType == "category") {
        if (m_categoryFilter.isEmpty()) return true;
        return sourceModel()->data(idx, MediaLibraryModel::CategoryRole).toString() == m_categoryFilter;
    }

    return true;
}
