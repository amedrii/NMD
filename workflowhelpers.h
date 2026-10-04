#pragma once
#include "queuedmod.h"
#include <QJsonDocument>

// Internal helpers shared by the window's workflow modules.
namespace nmd
{
inline QString downloadHistoryKey(const QueuedMod& entry)
{
    return entry.link.modKey() + "/" + entry.link.file;
}
inline QString modApiPath(const QueuedMod& entry)
{
    return "/v1/games/" + entry.link.game + "/mods/" + entry.link.mod;
}
inline QString formatFileSize(qint64 bytes)
{
    if (bytes <= 0)
    {
        return "Size unknown";
    }
    if (bytes < 1024)
    {
        return QString::number(bytes) + " B";
    }
    if (bytes < 1024 * 1024)
    {
        return QString::number(double(bytes) / 1024, 'f', 1) + " KB";
    }
    return QString::number(double(bytes) / 1024 / 1024, 'f', 1) + " MB";
}
inline QJsonObject responseObject(const QJsonDocument& document)
{
    const auto root = document.object();
    return root.contains("data") && root["data"].isObject() ? root["data"].toObject() : root;
}
} // namespace nmd
