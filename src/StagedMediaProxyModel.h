#pragma once

#include <QSortFilterProxyModel>
#include <QStringList>

/**
 * @brief The StagedMediaProxyModel class
 * Filters the global MediaLibraryModel based on the associatedMediaIds
 * of the currently selected MeetingSegment.
 */
class StagedMediaProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString selectedSegmentId READ selectedSegmentId WRITE setSelectedSegmentId NOTIFY selectedSegmentIdChanged)
    Q_PROPERTY(QString filterType READ filterType WRITE setFilterType NOTIFY filterChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY filterChanged)
    // Composes with filterType/categoryFilter (an extra AND condition), not
    // a mode of its own -- "all"/"video"/"image" lets Quick Fetch and Pins
    // narrow whatever view they're already showing down to one media kind.
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY filterChanged)
    Q_PROPERTY(QString languageCode READ languageCode WRITE setLanguageCode NOTIFY languageCodeChanged)
    Q_PROPERTY(QStringList stagedIds READ stagedIds WRITE setStagedIds NOTIFY stagedIdsChanged)

public:
    explicit StagedMediaProxyModel(QObject *parent = nullptr);

    QString selectedSegmentId() const { return m_selectedSegmentId; }
    void setSelectedSegmentId(const QString &id);

    QString filterType() const { return m_filterType; }
    void setFilterType(const QString &t);

    QString categoryFilter() const { return m_categoryFilter; }
    void setCategoryFilter(const QString &c);

    QString typeFilter() const { return m_typeFilter; }
    void setTypeFilter(const QString &t);

    QString languageCode() const { return m_languageCode; }
    void setLanguageCode(const QString &code);

    QStringList stagedIds() const { return m_stagedIds; }
    void setStagedIds(const QStringList &ids);

signals:
    void selectedSegmentIdChanged();
    void filterChanged();
    void languageCodeChanged();
    void stagedIdsChanged();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    // Only meaningful for filterType "segment"/"pinned": orders rows to
    // match m_stagedIds' position, i.e. the linked-media list's own order,
    // so drag-reordering that list (see MeetingScheduleModel::moveLinkedMedia)
    // is actually reflected in what the grid shows. Sorting is otherwise
    // disabled (see setFilterType), which preserves the previous "natural
    // source order" behavior for every other view.
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_selectedSegmentId;
    QString m_filterType = "segment";
    QString m_categoryFilter;
    QString m_typeFilter = "all";
    QString m_languageCode = "E";
    QStringList m_stagedIds;
};
