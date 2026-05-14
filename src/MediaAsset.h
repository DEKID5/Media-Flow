#pragma once

#include <QString>
#include <QMetaType>
#include <QObject>

/**
 * @brief Represents a single media file or hardware input.
 */
struct MediaAsset
{
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString type MEMBER type) // "video", "image", "audio", "input"
    Q_PROPERTY(QString absolutePath MEMBER absolutePath)
    Q_PROPERTY(QString thumbnailPath MEMBER thumbnailPath)
    Q_PROPERTY(bool isStaged MEMBER isStaged)
    Q_PROPERTY(int songNumber MEMBER songNumber)
    Q_PROPERTY(QString languageCode MEMBER languageCode)

public:
    QString id;
    QString name;
    QString type;
    QString absolutePath;
    QString thumbnailPath;
    bool isStaged = false;
    int songNumber = 0;
    QString languageCode;

    bool isSong() const { return songNumber > 0; }
};

Q_DECLARE_METATYPE(MediaAsset)
