#pragma once

#include <QAbstractListModel>
#include <QCameraDevice>
#include <QVector>

class CameraDeviceModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole };

    explicit CameraDeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString deviceIdAt(int row) const;
    Q_INVOKABLE QString nameAt(int row) const;

    QCameraDevice deviceForId(const QString &id) const;

private:
    QVector<QCameraDevice> m_devices;
};
