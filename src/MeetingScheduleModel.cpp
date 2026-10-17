#include "MeetingScheduleModel.h"
#include <QDebug>
#include <QTime>
#include <algorithm>

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

    rows = defaultRows(type);
}

QVector<MeetingScheduleModel::MeetingRow> MeetingScheduleModel::defaultRows(const QString &type)
{
    QVector<MeetingRow> rows;
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
    return rows;
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
    for (auto &r : m_midweekRows) { r.linkedMediaIds.clear(); r.workbookMediaIds.clear(); r.isLive = false; r.songNumber = 0; }
    for (auto &r : m_weekendRows) { r.linkedMediaIds.clear(); r.workbookMediaIds.clear(); r.isLive = false; r.songNumber = 0; }
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

void MeetingScheduleModel::setWorkbookMediaForId(const QString &id, const QStringList &mediaIds)
{
    ensurePopulated(QStringLiteral("midweek"));
    ensurePopulated(QStringLiteral("weekend"));

    auto update = [this, &id, &mediaIds](QVector<MeetingRow> &rows, const QString &type) {
        for (int i = 0; i < rows.size(); ++i) {
            MeetingRow &row = rows[i];
            if (row.id != id)
                continue;

            for (const QString &previousId : row.workbookMediaIds)
                row.linkedMediaIds.removeAll(previousId);
            row.workbookMediaIds = mediaIds;
            for (const QString &mediaId : mediaIds) {
                if (!row.linkedMediaIds.contains(mediaId))
                    row.linkedMediaIds.append(mediaId);
            }
            if (m_activeType == type)
                emit dataChanged(index(i), index(i), {AssociatedMediaIdsRole});
            return true;
        }
        return false;
    };

    if (update(m_midweekRows, QStringLiteral("midweek")))
        return;
    update(m_weekendRows, QStringLiteral("weekend"));
}

void MeetingScheduleModel::setWorkbookScheduleRows(const QVariantList &workbookRows)
{
    ensurePopulated(QStringLiteral("midweek"));

    QHash<QString, MeetingRow> existing;
    for (const MeetingRow &row : m_midweekRows)
        existing.insert(row.id, row);

    auto manualMedia = [&existing](const QString &id) {
        QStringList ids;
        const MeetingRow row = existing.value(id);
        for (const QString &mediaId : row.linkedMediaIds) {
            if (!row.workbookMediaIds.contains(mediaId))
                ids.append(mediaId);
        }
        return ids;
    };

    QHash<QString, QVector<MeetingRow>> assignments;
    for (const QVariant &value : workbookRows) {
        const QVariantMap map = value.toMap();
        const int number = map.value(QStringLiteral("number")).toInt();
        if (number < 4 || number > 9)
            continue;

        MeetingRow row;
        row.id = map.value(QStringLiteral("id")).toString();
        row.title = map.value(QStringLiteral("title")).toString();
        row.type = QStringLiteral("talk");
        row.durationMinutes = qMax(0, map.value(QStringLiteral("durationMinutes")).toInt());
        row.linkedMediaIds = manualMedia(row.id);
        if (row.id.isEmpty() || row.title.isEmpty())
            continue;

        const QString group = number <= 6 ? QStringLiteral("apply")
            : number == 7 ? QStringLiteral("living") : QStringLiteral("bibleStudy");
        assignments[group].append(row);
    }

    for (const QString &group : {QStringLiteral("apply"), QStringLiteral("living"),
                                 QStringLiteral("bibleStudy")}) {
        QVector<MeetingRow> &groupRows = assignments[group];
        std::sort(groupRows.begin(), groupRows.end(), [](const MeetingRow &a, const MeetingRow &b) {
            bool aOk = false;
            bool bOk = false;
            const int aNumber = a.id.section(QLatin1Char('_'), -1).toInt(&aOk);
            const int bNumber = b.id.section(QLatin1Char('_'), -1).toInt(&bOk);
            if (aOk && bOk && aNumber != bNumber)
                return aNumber < bNumber;
            return a.id < b.id;
        });
    }

    QVector<MeetingRow> rebuilt;
    const QVector<MeetingRow> defaults = defaultRows(QStringLiteral("midweek"));
    for (MeetingRow row : defaults) {
        const auto old = existing.constFind(row.id);
        if (old != existing.cend()) {
            row.songNumber = old->songNumber;
            row.isLive = old->isLive;
            if (row.id == QStringLiteral("m12"))
                row.title = old->title;
            if (workbookRows.isEmpty()
                && (row.id == QStringLiteral("m6") || row.id == QStringLiteral("m8")
                    || row.id == QStringLiteral("m9"))) {
                row.linkedMediaIds = manualMedia(row.id);
            }
            if (row.id != QStringLiteral("m6") && row.id != QStringLiteral("m8")
                && row.id != QStringLiteral("m9")) {
                for (const QString &mediaId : old->linkedMediaIds) {
                    if (!old->workbookMediaIds.contains(mediaId)
                        && !row.linkedMediaIds.contains(mediaId))
                        row.linkedMediaIds.append(mediaId);
                }
            }
        }

        if (row.id == QStringLiteral("m6")) {
            rebuilt += assignments.value(QStringLiteral("apply"));
            if (workbookRows.isEmpty())
                rebuilt.append(row);
            continue;
        }
        if (row.id == QStringLiteral("m8")) {
            rebuilt += assignments.value(QStringLiteral("living"));
            if (workbookRows.isEmpty())
                rebuilt.append(row);
            continue;
        }
        if (row.id == QStringLiteral("m9")) {
            rebuilt += assignments.value(QStringLiteral("bibleStudy"));
            if (workbookRows.isEmpty())
                rebuilt.append(row);
            continue;
        }
        rebuilt.append(row);
    }

    QTime meetingTime(19, 0);
    for (MeetingRow &row : rebuilt) {
        row.time = meetingTime.toString(QStringLiteral("HH:mm"));
        const int slotMinutes = row.isSong ? 5 : row.durationMinutes;
        if (slotMinutes > 0)
            meetingTime = meetingTime.addSecs(slotMinutes * 60);
    }

    beginResetModel();
    m_midweekRows = rebuilt;
    endResetModel();
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
        m.insert("workbookMediaIds", s.workbookMediaIds);
        m.insert("songNumber", s.songNumber);
        m.insert("durationMinutes", s.durationMinutes);
        m.insert("title", s.title);
        m.insert("time", s.time);
        m.insert("type", s.type);
        m.insert("isSong", s.isSong);
        list.append(m);
    }
    return list;
}

void MeetingScheduleModel::setFullState(const QString &type, const QVariantList &data)
{
    ensurePopulated(type);
    auto &rows = (type == "midweek") ? m_midweekRows : m_weekendRows;
    QHash<QString, MeetingRow> rowsById;
    for (const MeetingRow &row : rows)
        rowsById.insert(row.id, row);

    QVector<MeetingRow> restored;
    for (const QVariant &v : data) {
        QVariantMap m = v.toMap();
        const QString id = m.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;
        MeetingRow row = rowsById.value(id);
        if (row.id.isEmpty() && type == QStringLiteral("midweek")
            && id.startsWith(QStringLiteral("mwb_"))) {
            row.id = id;
            row.title = m.value(QStringLiteral("title")).toString();
            row.time = m.value(QStringLiteral("time")).toString();
            row.type = m.value(QStringLiteral("type"), QStringLiteral("talk")).toString();
        }
        if (row.id.isEmpty())
            continue;

        row.linkedMediaIds = m.value(QStringLiteral("mediaIds")).toStringList();
        row.workbookMediaIds = m.value(QStringLiteral("workbookMediaIds")).toStringList();
        row.songNumber = m.value(QStringLiteral("songNumber")).toInt();
        if (m.contains(QStringLiteral("durationMinutes")))
            row.durationMinutes = m.value(QStringLiteral("durationMinutes")).toInt();
        if (m.contains(QStringLiteral("title")))
            row.title = m.value(QStringLiteral("title")).toString();
        if (m.contains(QStringLiteral("time")))
            row.time = m.value(QStringLiteral("time")).toString();
        restored.append(row);
    }
    for (const MeetingRow &row : rows) {
        if (std::none_of(restored.cbegin(), restored.cend(), [&row](const MeetingRow &saved) {
                return saved.id == row.id;
            }))
            restored.append(row);
    }
    beginResetModel();
    rows = restored;
    endResetModel();
}

QVector<MeetingScheduleModel::MeetingRow> &MeetingScheduleModel::activeRows() { return (m_activeType == "midweek") ? m_midweekRows : m_weekendRows; }
const QVector<MeetingScheduleModel::MeetingRow> &MeetingScheduleModel::activeRows() const { return (m_activeType == "midweek") ? m_midweekRows : m_weekendRows; }
