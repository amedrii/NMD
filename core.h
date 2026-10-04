#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

struct ModLink
{
    QString game, mod, file;
    QString modKey() const
    {
        return game + "/" + mod;
    }
    QUrl page() const;
    bool valid() const
    {
        return !game.isEmpty() && !mod.isEmpty();
    }
};
ModLink parseModLink(const QString& text);
ModLink parseRequirementLink(
    const QJsonObject& requirement, const QString& parentGame, const QString& parentGameId);
bool validSignedNxm(const QUrl& url, QString* error = nullptr);
QString safeArchiveName(QString name);
QString jsonId(const QJsonValue& value);
QString archiveRelativePath(const ModLink& link, const QString& name);
bool isDownloadHost(const QUrl& url);
