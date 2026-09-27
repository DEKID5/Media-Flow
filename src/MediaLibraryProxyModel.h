#pragma once

#include <QSortFilterProxyModel>
#include <QString>

class MediaLibraryProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterCategory READ filterCategory WRITE setFilterCategory NOTIFY filterCategoryChanged)
    Q_PROPERTY(QString languageCode READ languageCode WRITE setLanguageCode NOTIFY languageCodeChanged)

public:
    explicit MediaLibraryProxyModel(QObject *parent = nullptr);

    QString filterCategory() const { return m_category; }
    void setFilterCategory(const QString &cat);

    QString languageCode() const { return m_languageCode; }
    void setLanguageCode(const QString &code);

signals:
    void filterCategoryChanged();
    void languageCodeChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_category;
    QString m_languageCode = "E";
};
