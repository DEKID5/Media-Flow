#include "StagedMediaProxyModel.h"
#include "MediaLibraryModel.h"
#include "SongSearchUtils.h"

StagedMediaProxyModel::StagedMediaProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    // Matches the default m_filterType ("segment") -- setFilterType() only
    // calls sort() on an actual change, which a QML binding whose first
    // evaluated value equals the C++ default won't trigger.
    sort(0);
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
        m_stagedIdSet.clear();
        m_stagedPositions.clear();
        m_stagedIdSet.reserve(ids.size());
        m_stagedPositions.reserve(ids.size());
        for (int i = 0; i < ids.size(); ++i) {
            const QString &id = ids.at(i);
            if (!m_stagedIdSet.contains(id))
                m_stagedPositions.insert(id, i);
            m_stagedIdSet.insert(id);
        }
        // Full invalidate (not just invalidateFilter): the *order* encoded
        // in m_stagedIds can change (a reorder) without any row's pass/fail
        // filter result changing, and only invalidate() re-runs lessThan().
        invalidate();
        emit stagedIdsChanged();
    }
}

void StagedMediaProxyModel::setFilterType(const QString &t)
{
    if (m_filterType != t) {
        m_filterType = t;
        invalidateFilter();
        // Sorting only makes sense (and is only implemented) for the
        // link-ordered views; everything else keeps the previous "no
        // sorting, natural source order" behavior (sortColumn -1).
        sort((t == "segment" || t == "pinned") ? 0 : -1);
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

void StagedMediaProxyModel::setTypeFilter(const QString &t)
{
    if (m_typeFilter != t) {
        m_typeFilter = t;
        invalidateFilter();
        emit filterChanged();
    }
}

void StagedMediaProxyModel::setLanguageCode(const QString &code)
{
    const QString normalized = SongSearchUtils::normalizeLanguageCode(code);
    if (m_languageCode != normalized) {
        m_languageCode = normalized;
        m_requiredLangMarker = QStringLiteral("_%1_").arg(m_languageCode);
        invalidateFilter();
        emit languageCodeChanged();
        emit filterChanged();
    }
}

bool StagedMediaProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);
    const auto *libModel = static_cast<const MediaLibraryModel*>(sourceModel());
    if (!libModel)
        return true;

    if (source_row < 0 || source_row >= libModel->rowCount())
        return false;

    const MediaLibraryModel::Row &row = libModel->rowAt(source_row);

    // 1. Quick check type filter first (e.g. ALL, VIDEO, IMAGE)
    if (m_typeFilter != QLatin1String("all") && row.type != m_typeFilter)
        return false;

    // 2. Check view / category filter next
    if (m_filterType == QLatin1String("category")) {
        if (!m_categoryFilter.isEmpty() && row.category != m_categoryFilter)
            return false;
    } else if (m_filterType == QLatin1String("segment")) {
        if (m_selectedSegmentId.isEmpty() || !m_stagedIdSet.contains(row.id))
            return false;
    } else if (m_filterType == QLatin1String("pinned")) {
        if (!m_stagedIdSet.contains(row.id))
            return false;
    } else if (m_filterType == QLatin1String("imported")) {
        if (!row.imported)
            return false;
    } else if (m_filterType == QLatin1String("videos")) {
        if (row.type != QLatin1String("video"))
            return false;
    } else if (m_filterType == QLatin1String("images")) {
        if (row.type != QLatin1String("image"))
            return false;
    } else if (m_filterType == QLatin1String("audio")) {
        if (row.type != QLatin1String("audio"))
            return false;
    }

    // 3. Language marker check (only for video/audio)
    if ((row.type == QLatin1String("video") || row.type == QLatin1String("audio")) && !row.languageMarker.isEmpty()) {
        if (row.languageMarker.compare(m_requiredLangMarker, Qt::CaseInsensitive) != 0)
            return false;
    }

    return true;
}

bool StagedMediaProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_filterType != "segment" && m_filterType != "pinned")
        return QSortFilterProxyModel::lessThan(left, right);

    const auto *libModel = static_cast<const MediaLibraryModel*>(sourceModel());
    if (!libModel)
        return left.row() < right.row();

    const QString &leftId = libModel->rowAt(left.row()).id;
    const QString &rightId = libModel->rowAt(right.row()).id;
    const int leftPos = m_stagedPositions.value(leftId, -1);
    const int rightPos = m_stagedPositions.value(rightId, -1);
    return leftPos < rightPos;
}
