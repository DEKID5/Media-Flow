#include "CameraDeviceModel.h"

#include <QMediaDevices>

CameraDeviceModel::CameraDeviceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    refresh();
}

int CameraDeviceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_devices.size();
}

QVariant CameraDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return {};

    const QCameraDevice &d = m_devices.at(index.row());
    switch (role) {
    case IdRole:
        return QString::fromUtf8(d.id());
    case NameRole:
        return d.description();
    default:
        return {};
    }
}

QHash<int, QByteArray> CameraDeviceModel::roleNames() const
{
    return {{IdRole, "deviceId"}, {NameRole, "deviceName"}};
}

void CameraDeviceModel::refresh()
{
    beginResetModel();
    m_devices = QMediaDevices::videoInputs();
    endResetModel();
}

QString CameraDeviceModel::deviceIdAt(int row) const
{
    if (row < 0 || row >= m_devices.size())
        return {};
    return QString::fromUtf8(m_devices.at(row).id());
}

QString CameraDeviceModel::nameAt(int row) const
{
    if (row < 0 || row >= m_devices.size())
        return {};
    return m_devices.at(row).description();
}

QCameraDevice CameraDeviceModel::deviceForId(const QString &id) const
{
    if (id.isEmpty())
        return {};
    for (const QCameraDevice &d : m_devices) {
        if (QString::fromUtf8(d.id()) == id)
            return d;
    }
    return {};
}
