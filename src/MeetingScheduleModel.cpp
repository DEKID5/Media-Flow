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
        case DurationMinutesRole: return s.durationMinutes;
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
    roles[DurationMinutesRole] = "durationMinutes";
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

    // durationMinutes for each non-song segment is just the gap to the next
    // segment's own scheduled time above -- e.g. Public Talk (10:05 -> 10:35)
    // is 30 min, Watchtower Study (10:40 -> 11:40) is 60 min -- so it can
    // never drift out of sync with the times already shown per row.
    if (type == "midweek") {
        rows << MeetingRow{.id = "m1", .time = "19:00", .title = "OPENING SONG", .type = "song", .isSong = true, .durationMinutes = 0}
             << MeetingRow{.id = "m3", .time = "19:05", .title = "TREASURES", .type = "talk", .durationMinutes = 10}
             << MeetingRow{.id = "m6", .time = "19:15", .title = "APPLY YOURSELF", .type = "talk", .durationMinutes = 15}
             << MeetingRow{.id = "m7", .time = "19:30", .title = "MIDDLE SONG", .type = "song", .isSong = true, .durationMinutes = 0}
             << MeetingRow{.id = "m8", .time = "19:35", .title = "LIVING AS CHRISTIANS", .type = "talk", .durationMinutes = 15}
             << MeetingRow{.id = "m9", .time = "19:50", .title = "CONGREGATION BIBLE STUDY", .type = "talk", .durationMinutes = 30}
             << MeetingRow{.id = "m12", .time = "20:20", .title = "ADDITIONAL PART", .type = "talk", .durationMinutes = 5}
             << MeetingRow{.id = "m11", .time = "20:25", .title = "CLOSING SONG", .type = "song", .isSong = true, .durationMinutes = 0};
    } else {
        rows << MeetingRow{.id = "w1", .time = "10:00", .title = "OPENING SONG", .type = "song", .isSong = true, .durationMinutes = 0}
             << MeetingRow{.id = "w2", .time = "10:05", .title = "PUBLIC TALK", .type = "talk", .durationMinutes = 30}
             << MeetingRow{.id = "w3", .time = "10:35", .title = "MIDDLE SONG", .type = "song", .isSong = true, .durationMinutes = 0}
             << MeetingRow{.id = "w4", .time = "10:40", .title = "WATCHTOWER STUDY", .type = "talk", .durationMinutes = 60}
             << MeetingRow{.id = "w5", .time = "11:40", .title = "CONCLUDING COMMENTS", .type = "talk", .durationMinutes = 3}
             << MeetingRow{.id = "w6", .time = "11:43", .title = "CLOSING SONG", .type = "song", .isSong = true, .durationMinutes = 0};
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

void MeetingScheduleModel::addLinkedMediaForId(const QString &id, const QString &mediaId)
{
    auto &mid = m_midweekRows;
    for (int i = 0; i < mid.size(); ++i) {
        if (mid[i].id == id) {
            if (!mid[i].linkedMediaIds.contains(mediaId)) {
                mid[i].linkedMediaIds.append(mediaId);
                if (m_activeType == "midweek") emit dataChanged(index(i), index(i), {AssociatedMediaIdsRole});
            }
            return;
        }
    }
    auto &wk = m_weekendRows;
    for (int i = 0; i < wk.size(); ++i) {
        if (wk[i].id == id) {
            if (!wk[i].linkedMediaIds.contains(mediaId)) {
                wk[i].linkedMediaIds.append(mediaId);
                if (m_activeType == "weekend") emit dataChanged(index(i), index(i), {AssociatedMediaIdsRole});
            }
            return;
        }
    }
}

void MeetingScheduleModel::setLinkedMedia(int row, const QStringList &mediaIds)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    rows[row].linkedMediaIds = mediaIds;
    emit dataChanged(index(row), index(row), {AssociatedMediaIdsRole});
}

void MeetingScheduleModel::setLinkedMediaForId(const QString &id, const QStringList &mediaIds)
{
    auto &mid = m_midweekRows;
    for (int i = 0; i < mid.size(); ++i) {
        if (mid[i].id == id) {
            mid[i].linkedMediaIds = mediaIds;
            if (m_activeType == "midweek") emit dataChanged(index(i), index(i), {AssociatedMediaIdsRole});
            return;
        }
    }
    auto &wk = m_weekendRows;
    for (int i = 0; i < wk.size(); ++i) {
        if (wk[i].id == id) {
            wk[i].linkedMediaIds = mediaIds;
            if (m_activeType == "weekend") emit dataChanged(index(i), index(i), {AssociatedMediaIdsRole});
            return;
        }
    }
}

void MeetingScheduleModel::removeLinkedMedia(int row, const QString &mediaId)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    rows[row].linkedMediaIds.removeAll(mediaId);
    emit dataChanged(index(row), index(row), {AssociatedMediaIdsRole});
}

void MeetingScheduleModel::moveLinkedMedia(int row, int fromIndex, int toIndex)
{
    auto &rows = activeRows();
    if (row < 0 || row >= rows.size()) return;
    QStringList &ids = rows[row].linkedMediaIds;
    if (fromIndex < 0 || fromIndex >= ids.size() || toIndex < 0 || toIndex >= ids.size() || fromIndex == toIndex)
        return;
    ids.move(fromIndex, toIndex);
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

void MeetingScheduleModel::setSongNumberForId(const QString &id, int songNum)
{
    auto &mid = m_midweekRows;
    for (int i = 0; i < mid.size(); ++i) {
        if (mid[i].id == id) {
            mid[i].songNumber = songNum;
            if (m_activeType == "midweek") emit dataChanged(index(i), index(i), {SongNumberRole});
            return;
        }
    }
    auto &wk = m_weekendRows;
    for (int i = 0; i < wk.size(); ++i) {
        if (wk[i].id == id) {
            wk[i].songNumber = songNum;
            if (m_activeType == "weekend") emit dataChanged(index(i), index(i), {SongNumberRole});
            return;
        }
    }
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
        m.insert("durationMinutes", s.durationMinutes);
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
                // Older saved states won't have this key -- keep
                // ensurePopulated's default duration rather than zeroing it.
                if (m.contains("durationMinutes"))
                    r.durationMinutes = m["durationMinutes"].toInt();
                break;
            }
        }
    }
    if (m_activeType == type) emit dataChanged(index(0), index(rows.size() - 1));
}

QVector<MeetingScheduleModel::MeetingRow> &MeetingScheduleModel::activeRows() { return (m_activeType == "midweek") ? m_midweekRows : m_weekendRows; }
const QVector<MeetingScheduleModel::MeetingRow> &MeetingScheduleModel::activeRows() const { return (m_activeType == "midweek") ? m_midweekRows : m_weekendRows; }
