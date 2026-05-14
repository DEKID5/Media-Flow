#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief The MeetingScheduleModel class (SequenceModel)
 * Holds two separate chronological meeting sequences: midweek and weekend.
 * Switching tabs swaps the active data but preserves both in memory.
 */
class MeetingScheduleModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString activeMeetingType READ activeMeetingType NOTIFY meetingTypeChanged)

public:
    struct MeetingRow {
        QString id;
        QString time;
        QString title;
        QString type;      // "song", "talk", "generic"
        bool isSong = false;
        bool isLive = false;
        int songNumber = 0;
        QStringList linkedMediaIds;
    };

    enum Roles {
        IdRole = Qt::UserRole + 1,
        TimeRole,
        TitleRole,
        TypeRole,
        IsSongRole,
        IsLiveRole,
        AssociatedMediaIdsRole,
        SongNumberRole
    };

    explicit MeetingScheduleModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString activeMeetingType() const { return m_activeType; }

    // --- Actions ---
    void loadMeeting(const QString &meetingType);
    void setLinkedMedia(int row, const QStringList &mediaIds);
    void addLinkedMedia(int row, const QString &mediaId);
    Q_INVOKABLE void removeLinkedMedia(int row, const QString &mediaId);
    Q_INVOKABLE void clearAllMedia();
    Q_INVOKABLE void updateSegmentTitle(const QString &id, const QString &newTitle);
    void setSongNumber(int row, int songNum);
    void setIsLive(int row, bool live);
    void setActiveRow(int row);

    /**
     * @brief Clears all linked media from all segments in the active schedule.
     */
    QString rowIdAt(int row) const;
    int rowOfId(const QString &id) const;
 
    QVariantList getFullState(const QString &type) const;
    void setFullState(const QString &type, const QVariantList &data);
    

signals:
    void meetingTypeChanged();

private:
    void ensurePopulated(const QString &type);

    QVector<MeetingRow> m_midweekRows;
    QVector<MeetingRow> m_weekendRows;
    QString m_activeType = "midweek";

    QVector<MeetingRow> &activeRows();
    const QVector<MeetingRow> &activeRows() const;
};
