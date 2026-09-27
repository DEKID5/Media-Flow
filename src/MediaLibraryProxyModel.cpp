#include "MediaLibraryProxyModel.h"
#include "MediaLibraryModel.h"

MediaLibraryProxyModel::MediaLibraryProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void MediaLibraryProxyModel::setFilterCategory(const QString &cat)
{
    if (m_category != cat) {
        m_category = cat;
        invalidateFilter();
        emit filterCategoryChanged();
    }
}

void MediaLibraryProxyModel::setLanguageCode(const QString &code)
{
    if (m_languageCode != code) {
        m_languageCode = code;
        invalidateFilter();
        emit languageCodeChanged();
    }
}

bool MediaLibraryProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    
    // 1. Filter by Category
    if (!m_category.isEmpty()) {
        QString cat = sourceModel()->data(index, MediaLibraryModel::CategoryRole).toString();
        if (cat != m_category) return false;
    }

    // 2. Filter by Language (Only for Songs/Videos that follow the pattern)
    QString name = sourceModel()->data(index, MediaLibraryModel::NameRole).toString();
    QString path = sourceModel()->data(index, MediaLibraryModel::PathRole).toString();
    
    // If it looks like a JW song (sjjm_)
    if (name.contains("sjjm", Qt::CaseInsensitive)) {
        // Construct the expected language marker, e.g., "_E_" or "_EW_"
        QString langMarker = QString("_%1_").arg(m_languageCode);
        
        // If the filename does not contain the current language marker, hide it
        if (!name.contains(langMarker, Qt::CaseInsensitive) && !path.contains(langMarker, Qt::CaseInsensitive)) {
            return false;
        }
    }

    return true;
}
