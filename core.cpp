#include "core.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QUrlQuery>

QString jsonId(const QJsonValue& value)
{
    return value.isString() ? value.toString() : QString::number(value.toInteger());
}
QUrl ModLink::page() const
{
    QUrl url("https://www.nexusmods.com/" + game + "/mods/" + mod);
    QUrlQuery query;
    query.addQueryItem("tab", "files");
    if (!file.isEmpty())
    {
        query.addQueryItem("file_id", file);
    }
    url.setQuery(query);
    return url;
}
ModLink parseModLink(const QString& text)
{
    const QUrl url(text.trimmed(), QUrl::StrictMode);
    if (!url.isValid() || !url.userInfo().isEmpty() || url.port() != -1)
    {
        return {};
    }
    static const QRegularExpression web("^/(?:games/)?([a-z0-9_]+)/mods/([1-9][0-9]*)/?$",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression nxm("^/mods/([1-9][0-9]*)/files/([1-9][0-9]*)/?$");
    static const QRegularExpression game("^[a-z0-9_]+$"), id("^[1-9][0-9]*$");
    if (url.scheme() == "nxm")
    {
        const auto m = nxm.match(url.path());
        if (m.hasMatch() && game.match(url.host()).hasMatch())
        {
            return {url.host().toLower(), m.captured(1), m.captured(2)};
        }
    }
    else if (url.scheme() == "https" &&
             (url.host() == "www.nexusmods.com" || url.host() == "nexusmods.com"))
    {
        const auto m = web.match(url.path());
        const QString file = QUrlQuery(url).queryItemValue("file_id");
        if (m.hasMatch() && (file.isEmpty() || id.match(file).hasMatch()))
        {
            return {m.captured(1).toLower(), m.captured(2), file};
        }
    }
    return {};
}
ModLink parseRequirementLink(
    const QJsonObject& requirement, const QString& parentGame, const QString& parentGameId)
{
    // Requirement metadata can use relative URLs or legacy HTTP links. Normalize
    // those to HTTPS; parseModLink still enforces the exact Nexus host and IDs.
    QString address = requirement["url"].toString().trimmed();
    if (address.startsWith("//"))
    {
        address.prepend("https:");
    }
    else if (address.startsWith('/'))
    {
        address.prepend("https://www.nexusmods.com");
    }
    else if (address.startsWith("www.nexusmods.com/", Qt::CaseInsensitive) ||
             address.startsWith("nexusmods.com/", Qt::CaseInsensitive))
    {
        address.prepend("https://");
    }
    QUrl url(address, QUrl::StrictMode);
    if (url.scheme() == "http")
    {
        url.setScheme("https");
    }
    auto link = parseModLink(url.toString());
    const QString modId = jsonId(requirement["modId"]);
    if (link.valid() && url.scheme() == "https" && link.mod == modId)
    {
        return link;
    }
    // Use structured identities if Nexus supplies a missing or unrecognised URL.
    // Never infer that a requirement belongs to its parent's game: IDs are scoped
    // by game, and cross-game requirements must not resolve to an unrelated mod.
    if (!requirement["gameId"].isNull() && !requirement["gameId"].isUndefined() &&
        jsonId(requirement["gameId"]) == parentGameId)
    {
        return parseModLink("https://www.nexusmods.com/" + parentGame + "/mods/" + modId);
    }
    return {};
}

bool validSignedNxm(const QUrl& url, QString* error)
{
    auto fail = [error](const QString& message)
    {
        if (error)
        {
            *error = message;
        }
        return false;
    };
    if (url.scheme() != "nxm" || !parseModLink(url.toString()).valid())
    {
        return fail("Paste a Nexus Mod Manager download link (nxm://...).");
    }
    const QUrlQuery query(url);
    bool ok = false;
    const qint64 expiry = query.queryItemValue("expires").toLongLong(&ok);
    if (query.queryItemValue("key").isEmpty() || !ok)
    {
        return fail(
            "This link has no download authorization. Click Mod manager download on Nexus first.");
    }
    if (expiry <= QDateTime::currentSecsSinceEpoch())
    {
        return fail("This download link expired. Confirm the download on Nexus again.");
    }
    return true;
}
QString safeArchiveName(QString name)
{
    name.replace(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1f]"), "_");
    name = name.left(160);
    while (name.endsWith('.') || name.endsWith(' '))
    {
        name.chop(1);
    }
    if (name.isEmpty() || name == "." || name == "..")
    {
        name = "archive.bin";
    }
    static const QRegularExpression reserved(
        "^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])($|\\.)", QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(name).hasMatch())
    {
        name.prepend('_');
    }
    return name;
}
QString archiveRelativePath(const ModLink& link, const QString& name)
{
    return link.game + "/" + link.mod + "/" + link.file + "-" + safeArchiveName(name);
}
bool isDownloadHost(const QUrl& url)
{
    const QString host = url.host().toLower();
    return url.scheme() == "https" && url.userInfo().isEmpty() && url.port() == -1 &&
           (host == "nexusmods.com" || host.endsWith(".nexusmods.com"));
}
