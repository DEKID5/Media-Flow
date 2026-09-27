#include "MeetingScheduleModel.h"
#include <QDebug>

MeetingScheduleModel::MeetingScheduleModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MeetingScheduleModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return activeRows().size();
}

QVariant MeetingScheduleModel::data(const QModelIndex &index, int role) const
{
    const auto &rows = activeRows();
    if (!index.isValid() || index.row() >= rows.size()) return {};
    const auto &s = rows[index.row()];
    switch (role) {
        case IdRole: return s.id;
        case TimeRole: return s.time;
        case TitleRole: return s.title;
        case TypeRole: return s.type;
        case IsSongRole: return s.isSong;
        case IsLiveRole: return s.isLive;
        case AssociatedMediaIdsRole: return s.linkedMediaIds;
        case SongNumberRole: return s.songNumber;
    }
    return {};
}

QHash<int, QByteArray> MeetingScheduleModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TimeRole] = "time";
    roles[TitleRole] = "title";
    roles[TypeRole] = "type";
    roles[IsSongRole] = "isSong";
    roles[IsLiveRole] = "isLive";
    roles[AssociatedMediaIdsRole] = "associatedMediaIds";
    roles[SongNumberRole] = "songNumber";
    return roles;
}

void MeetingScheduleModel::loadMeeting(const QString &meetingType)
{
    m_activeType = meetingType;
    ensurePopulated(meetingType);
    beginResetModel();
    endResetModel();
    emit meetingTypeChanged();
}

void MeetingScheduleModel::ensurePopulated(const QString &type)
{
    auto &rows = (type == "midweek") ? m_midweekRows : m_weekendRows;
    if (!rows.isEmpty()) return;

    if (type == "midweek") {
        rows << MeetingRow{"m1", "19:00", "OPENING SONG", "song", true}
             << MeetingRow{"m3", "19:05", "TREASURES", "talk"}
             << MeetingRow{"m6", "19:15", "APPLY YOURSELF", "talk"}
             << MeetingRow{"m7", "19:30", "MIDDLE SONG", "song", true}
             << MeetingRow{"m8", "19:35", "LIVING AS CHRISTIANS", "talk"}
             << MeetingRow{"m9", "19:50", "CONGREGATION BIBLE STUDY", "talk"}
             << MeetingRow{"m12", "20:20", "ADDITIONAL PART", "talk"}
             << MeetingRow{"m11", "20:25", "CLOSING SONG", "song", true};
    } else {
        rows << MeetingRow{"w1", "10:00", "OPENING SONG", "song", true}
             << MeetingRow{"w2", "10:05", "PUBLIC TALK", "talk"}
             << MeetingRow{"w3", "10:35", "MIDDLE SONG", "song", true}
             << MeetingRow{"w4", "10:40", "WATCHTOWER STUDY", "talk"}
             << MeetingRow{"w5", "11:40", "CONCLUDING COMMENTS", "talk"}
             << MeetingRow{"w6", "11:43", "CLOSING SONG", "song", true};
    }
}

void MeetingScheduleModel::addLinkedMedia(int row, const QString &mediaId)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    if (!rows[row].linkedMediaIds.contains(mediaId)) {
        rows[row].linkedMediaIds.append(mediaId);
        emit dataChanged(index(row), index(row), {AssociatedMediaIdsRole});
    }
}

void MeetingScheduleModel::setLinkedMedia(int row, const QStringList &mediaIds)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    rows[row].linkedMediaIds = mediaIds;
    emit dataChanged(index(row), index(row), {AssociatedMediaIdsRole});
}

void MeetingScheduleModel::removeLinkedMedia(int row, const QString &mediaId)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    rows[row].linkedMediaIds.removeAll(mediaId);
    emit dataChanged(index(row), index(row), {AssociatedMediaIdsRole});
}

void MeetingScheduleModel::clearAllMedia()
{
    beginResetModel();
    for (auto &r : m_midweekRows) { r.linkedMediaIds.clear(); r.isLive = false; r.songNumber = 0; }
    for (auto &r : m_weekendRows) { r.linkedMediaIds.clear(); r.isLive = false; r.songNumber = 0; }
    endResetModel();
}

void MeetingScheduleModel::updateSegmentTitle(const QString &id, const QString &newTitle)
{
    auto &mid = m_midweekRows;
    for (int i = 0; i < mid.size(); ++i) {
        if (mid[i].id == id) {
            mid[i].title = newTitle;
            if (m_activeType == "midweek") emit dataChanged(index(i), index(i), {TitleRole});
            return;
        }
    }
    auto &wk = m_weekendRows;
    for (int i = 0; i < wk.size(); ++i) {
        if (wk[i].id == id) {
            wk[i].title = newTitle;
            if (m_activeType == "weekend") emit dataChanged(index(i), index(i), {TitleRole});
            return;
        }
    }
}

void MeetingScheduleModel::setSongNumber(int row, int songNum)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    rows[row].songNumber = songNum;
    emit dataChanged(index(row), index(row), {SongNumberRole});
}

void MeetingScheduleModel::setIsLive(int row, bool live)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    rows[row].isLive = live;
    emit dataChanged(index(row), index(row), {IsLiveRole});
}

void MeetingScheduleModel::setActiveRow(int row) { /* Not used in this version */ }

int MeetingScheduleModel::rowOfId(const QString &id) const
{
    const auto &rows = activeRows();
    for (int i = 0; i < rows.size(); ++i) {
        if (rows[i].id == id) return i;
    }
    return -1;
}

QVariantList MeetingScheduleModel::getFullState(const QString &type) const
{
    const auto &rows = (type == "midweek") ? m_midweekRows : m_weekendRows;
    QVariantList list;
    for (const auto &s : rows) {
        QVariantMap m;
        m.insert("id", s.id);
        m.insert("mediaIds", s.linkedMediaIds);
        m.insert("songNumber", s.songNumber);
        list.append(m);
    }
    return list;
}

void MeetingScheduleModel::setFullState(const QString &type, const QVariantList &data)
{
    ensurePopulated(type);
    auto &rows = (type == "midweek") ? m_midweekRows : m_weekendRows;
    for (const QVariant &v : data) {
        QVariantMap m = v.toMap();
        QString id = m["id"].toString();
        for (auto &r : rows) {
            if (r.id == id) {
                r.linkedMediaIds = m["mediaIds"].toStringList();
                r.songNumber = m["songNumber"].toInt();
                break;
            }
        }
    }
    if (m_activeType == type) emit dataChanged(index(0), index(rows.size() - 1));
}

QVector<MeetingScheduleModel::MeetingRow> &MeetingScheduleModel::activeRows() { return (m_activeType == "midweek") ? m_midweekRows : m_weekendRows; }
const QVector<MeetingScheduleModel::MeetingRow> &MeetingScheduleModel::activeRows() const { return (m_activeType == "midweek") ? m_midweekRows : m_weekendRows; }
