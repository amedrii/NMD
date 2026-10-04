#include "mainwindow.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtWidgets>
#include <cstring>
#include <iostream>

static int failures = 0;
static int checks = 0;
static void expect(bool condition, const char* name)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << name << "\n";
    }
}
static bool waitUntil(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000)
    {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}
struct Response
{
    QByteArray data;
    int code = 200;
    QByteArray type = "application/json";
    int delay = 0;
};
class FakeReply : public QNetworkReply
{
    QByteArray body;
    qint64 position = 0;
    bool done = false;

  public:
    FakeReply(const QNetworkRequest& request, Response response, QObject* parent)
        : QNetworkReply(parent), body(response.data)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.code);
        setHeader(QNetworkRequest::ContentTypeHeader, response.type);
        setHeader(QNetworkRequest::ContentLengthHeader, body.size());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(response.delay,
            this,
            [this, response]
        {
            if (done)
            {
                return;
            }
            if (response.code >= 400)
            {
                setError(QNetworkReply::ContentAccessDenied, "Fixture error");
            }
            done = true;
            setFinished(true);
            emit readyRead();
            emit downloadProgress(body.size(), body.size());
            emit finished();
        });
    }
    void abort() override
    {
        if (done)
        {
            return;
        }
        done = true;
        setError(QNetworkReply::OperationCanceledError, "Canceled");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override
    {
        return body.size() - position + QNetworkReply::bytesAvailable();
    }
    bool isSequential() const override
    {
        return true;
    }

  protected:
    qint64 readData(char* data, qint64 max) override
    {
        const qint64 count = qMin(max, body.size() - position);
        if (count <= 0)
        {
            return -1;
        }
        std::memcpy(data, body.constData() + position, size_t(count));
        position += count;
        return count;
    }
};
class FakeNetwork : public QNetworkAccessManager
{
  public:
    bool brokenArchive = false;
    int archives = 0, downloadLinks = 0, delay = 0, pages = 0;
    QNetworkReply* createRequest(
        Operation, const QNetworkRequest& request, QIODevice* outgoing = nullptr) override
    {
        const QString path = request.url().path();
        Response response;
        response.delay = delay;
        if (request.url().host() == "files.nexusmods.com")
        {
            ++archives;
            expect(request.rawHeader("apikey").isEmpty(),
                "API credential is never sent to the archive server");
            response.type = "application/zip";
            response.data = brokenArchive ? QByteArray("bad") : QByteArray("archive");
            return new FakeReply(request, response, this);
        }
        expect(request.rawHeader("apikey") == "test-key", "API calls identify the local account");
        expect(request.rawHeader("Application-Name") == "NMD", "API calls identify this app");
        QJsonValue value;
        if (path == "/v1/users/validate.json")
        {
            value = QJsonObject{{"user_id", 42}, {"name", "Test account"}};
        }
        else if (path == "/v2/graphql")
        {
            auto body = QJsonDocument::fromJson(outgoing->readAll()).object();
            const auto variables = body["variables"].toObject();
            const auto mod = variables["mod"].toString();
            const auto offset = variables["offset"].toInt();
            QStringList required;
            if (mod == "1")
            {
                required = {"2", "3"};
            }
            else if (mod == "2")
            {
                required = {"1"}; // Cycle.
            }
            else if (mod == "3")
            {
                required = {"2"}; // Shared dependency.
            }
            else if (mod == "7")
            {
                ++pages;
                required = {offset == 0 ? "8" : "9"};
            }
            QJsonArray nodes;
            for (const auto& id : required)
            {
                nodes.append(QJsonObject{{"externalRequirement", false},
                    {"modId", id},
                    {"modName", "Mod " + id},
                    {"notes", "Author note"},
                    {"url", "https://www.nexusmods.com/testgame/mods/" + id}});
            }
            const int total = mod == "7" ? 2 : nodes.size();
            value = QJsonObject{{"data",
                QJsonObject{{"mod",
                    QJsonObject{{"legacyModRequirementsEnabled", mod != "4" && mod != "6"},
                        {"modRequirements",
                            QJsonObject{{"nexusRequirements",
                                            QJsonObject{{"nodes", nodes}, {"totalCount", total}}},
                                {"dlcRequirements", QJsonArray{}}}}}}}}};
        }
        else if (path.startsWith("/v3/games/"))
        {
            value =
                QJsonObject{{"data", QJsonObject{{"id", path.endsWith("/60") ? "600" : "400"}}}};
        }
        else if (path.endsWith("/dependencies"))
        {
            value = QJsonObject{{"dependency_definitions", QJsonArray{QJsonObject{{"id", "dep1"}}}},
                {"dlc_dependency_definitions", QJsonArray{}}};
        }
        else if (path.endsWith("/materialized"))
        {
            QJsonArray dependencies;
            if (!path.contains("/600/"))
            {
                dependencies.append(QJsonObject{{"id", "dep1"},
                    {"candidate_mod_files",
                        QJsonArray{QJsonObject{{"id", "file5"},
                            {"name", "Required file"},
                            {"mod",
                                QJsonObject{{"game_scoped_id", "5"},
                                    {"name", "Mod 5"},
                                    {"game", QJsonObject{{"domain_name", "testgame"}}}}},
                            {"candidate_versions",
                                QJsonArray{QJsonObject{{"game_scoped_id", "50"},
                                    {"category", "main"},
                                    {"version", "1.0"}}}}}}}});
            }
            value = QJsonObject{{"dependencies", dependencies}};
        }
        else if (path.endsWith("/download_link.json"))
        {
            ++downloadLinks;
            expect(QUrlQuery(request.url()).queryItemValue("key") == "authorized",
                "Signed download token is forwarded to Nexus");
            value = QJsonArray{QJsonObject{{"URI", "https://files.nexusmods.com/archive.zip"}}};
        }
        else
        {
            const auto match = QRegularExpression("/mods/([0-9]+)").match(path);
            const auto id = match.captured(1);
            if (path.endsWith("/files.json"))
            {
                value = QJsonObject{{"files",
                    QJsonArray{QJsonObject{{"file_id", id.toInt() * 10},
                        {"name", "Main archive"},
                        {"file_name", "mod.zip"},
                        {"category_id", 1},
                        {"category_name", "MAIN"},
                        {"size_in_bytes", 7},
                        {"version", "1.0"}}}}};
            }
            else if (path.endsWith(".json") && !id.isEmpty())
            {
                value = QJsonObject{{"name", "Mod " + id}, {"game_id", 123}};
            }
            else
            {
                response.code = 404;
                value = QJsonObject{{"error", "Unknown fixture path"}};
            }
        }
        response.data = value.isArray() ? QJsonDocument(value.toArray()).toJson()
                                        : QJsonDocument(value.toObject()).toJson();
        return new FakeReply(request, response, this);
    }
};
class IntegrationTests
{
  public:
    static void run()
    {
        QTemporaryDir dir;
        expect(dir.isValid(), "Isolated test folder exists");
        FakeNetwork fake;
        MainWindow window(&fake, dir.path() + "/queue.json");
        window.m_apiKeyInput->setText("test-key");
        window.m_downloadFolderInput->setText(dir.path() + "/downloads");
        window.m_modLinksInput->setPlainText(
            "https://www.nexusmods.com/testgame/mods/1\nhttps://www.nexusmods.com/testgame/mods/"
            "3\nhttps://www.nexusmods.com/testgame/mods/1");
        window.collectMods();
        expect(waitUntil([&] { return !window.m_isResolving; }), "Recursive resolution finishes");
        expect(window.m_queue.size() == 3,
            "Root duplicates, shared dependencies and cycles are deduplicated");
        for (const auto& entry : window.m_queue)
        {
            expect(entry->state == "Ready to confirm", "Each resolved legacy mod is ready");
            expect(!entry->link.file.isEmpty(), "Single main files are selected");
            expect(entry->version == "1.0", "Selected Nexus file version is tracked");
        }
        auto signedLink = [](int mod, int file, int user = 42)
        {
            return QString("nxm://testgame/mods/%1/files/%2?key=authorized&expires=%3&user_id=%4")
                .arg(mod)
                .arg(file)
                .arg(QDateTime::currentSecsSinceEpoch() + 3600)
                .arg(user);
        };
        window.acceptDownloadLink(signedLink(1, 10));
        expect(waitUntil([&] { return !window.m_isDownloading && !window.m_isResolving; }),
            "Authorized archive download finishes");
        QFile archive(dir.path() + "/downloads/testgame/1/10-mod.zip");
        expect(archive.open(QIODevice::ReadOnly) && archive.readAll() == "archive",
            "Complete archive is saved in the selected folder");
        archive.close();
        expect(window.m_downloadHistory.contains("testgame/1/10"),
            "Only completed downloads enter history");
        expect(window.m_downloadHistory["testgame/1/10"].toObject()["version"].toString() == "1.0",
            "Completed history records the Nexus file version");
        auto changedHistory = window.m_downloadHistory["testgame/1/10"].toObject();
        changedHistory["version"] = "0.9";
        window.m_downloadHistory["testgame/1/10"] = changedHistory;
        expect(!window.hasCompletedDownload(window.m_queue[0]),
            "A different Nexus file version is not treated as a duplicate");
        changedHistory["version"] = "1.0";
        window.m_downloadHistory["testgame/1/10"] = changedHistory;
        window.acceptDownloadLink(signedLink(1, 10));
        expect(fake.archives == 1, "Completed archive is skipped on repeated authorization");
        window.acceptDownloadLink(signedLink(2, 20, 99));
        expect(waitUntil([&] { return !window.m_isDownloading; }),
            "Wrong-account authorization is rejected");
        expect(fake.archives == 1, "Wrong account cannot start a download");
        fake.brokenArchive = true;
        window.acceptDownloadLink(signedLink(2, 20));
        expect(
            waitUntil([&] { return !window.m_isDownloading; }), "Truncated archive check finishes");
        expect(!QFileInfo::exists(dir.path() + "/downloads/testgame/2/20-mod.zip"),
            "Truncated archive is not committed");
        expect(!window.m_downloadHistory.contains("testgame/2/20"),
            "Truncated archive is not recorded as complete");
        fake.brokenArchive = false;
        window.acceptDownloadLink(signedLink(2, 20));
        expect(waitUntil([&] { return !window.m_isDownloading; }),
            "Failed download can be retried with a new authorization");
        expect(window.m_downloadHistory.contains("testgame/2/20"), "Retry completes successfully");
        // Existing untracked files must survive.
        QDir().mkpath(dir.path() + "/downloads/testgame/3");
        QFile existing(dir.path() + "/downloads/testgame/3/30-mod.zip");
        expect(existing.open(QIODevice::WriteOnly), "Collision fixture is created");
        existing.write("keep me");
        existing.close();
        window.acceptDownloadLink(signedLink(3, 30));
        expect(waitUntil([&] { return !window.m_isDownloading; }), "Collision check finishes");
        expect(existing.open(QIODevice::ReadOnly), "Existing file can be checked");
        expect(existing.readAll() == "keep me", "Untracked existing archive is never overwritten");
        existing.close();
        window.saveQueue();
        QFile state(dir.path() + "/queue.json");
        expect(state.open(QIODevice::ReadOnly), "Queue state can be checked");
        const auto saved = state.readAll();
        expect(!saved.contains("test-key") && !saved.contains("authorized"),
            "Persisted queue contains no API keys or signed links");
        FakeNetwork restoredNetwork;
        MainWindow restored(&restoredNetwork, dir.path() + "/queue.json");
        expect(restored.m_queue.size() == 3, "Queue persists across restarts");
        expect(restored.m_queue[0]->downloaded, "History recognizes completed files after restart");
        FakeNetwork modern;
        MainWindow modernWindow(&modern, dir.path() + "/modern.json");
        modernWindow.m_apiKeyInput->setText("test-key");
        modernWindow.m_downloadFolderInput->setText(dir.path() + "/modern-downloads");
        modernWindow.m_modLinksInput->setPlainText("https://www.nexusmods.com/testgame/mods/4");
        modernWindow.collectMods();
        expect(waitUntil([&] { return !modernWindow.m_isResolving; }),
            "File-level dependency resolution finishes");
        expect(modernWindow.m_queue.size() == 2, "File-level requirement is added recursively");
        expect(modernWindow.m_queue.last()->link.file == "50",
            "Version-specific requirement remains pinned");
        modernWindow.m_modLinksInput->setPlainText("https://www.nexusmods.com/testgame/mods/6");
        modernWindow.collectMods();
        expect(waitUntil([&] { return !modernWindow.m_isResolving; }),
            "Unavailable requirement check finishes");
        expect(modernWindow.m_queue.last()->state == "Needs attention",
            "Missing materialized requirements are not silently treated as satisfied");
        FakeNetwork paged;
        MainWindow pageWindow(&paged, dir.path() + "/page.json");
        pageWindow.m_apiKeyInput->setText("test-key");
        pageWindow.m_downloadFolderInput->setText(dir.path() + "/page-downloads");
        pageWindow.m_modLinksInput->setPlainText("https://www.nexusmods.com/testgame/mods/7");
        pageWindow.collectMods();
        expect(waitUntil([&] { return !pageWindow.m_isResolving; }),
            "Paginated requirement resolution finishes");
        expect(paged.pages == 2 && pageWindow.m_queue.size() == 3,
            "Every requirement page is fetched");
        paged.delay = 50;
        pageWindow.m_modLinksInput->setPlainText("https://www.nexusmods.com/testgame/mods/10");
        pageWindow.collectMods();
        pageWindow.stopWork();
        QCoreApplication::processEvents();
        expect(
            !pageWindow.m_isResolving && !pageWindow.m_isDownloading, "Stop cancels pending work");
        // Capture the real widget layout with simulated queue data for visual review.
        window.m_accountStatus->setText("Preview with simulated Nexus data");
        window.show();
        QCoreApplication::processEvents();
        window.grab().save(QCoreApplication::applicationDirPath() + "/app-preview.png");
        window.hide();
    }
};
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisb.ttf");
    QCoreApplication::setOrganizationName("NMD-Tests");
    QCoreApplication::setApplicationName("NMD-Tests");
    expect(parseModLink("https://www.nexusmods.com/skyrimspecialedition/mods/266?tab=files")
                   .modKey() == "skyrimspecialedition/266",
        "Canonical mod links parse");
    expect(parseModLink("https://nexusmods.com/fallout4/mods/1?file_id=20").file == "20",
        "File-specific links parse");
    expect(
        parseModLink("https://nexusmods.com/game/mods/1/").valid(), "Trailing slash is accepted");
    expect(parseModLink("https://www.nexusmods.com/games/monsterhunterwilds/mods/93").modKey() ==
               "monsterhunterwilds/93",
        "New Nexus game-prefixed mod links parse");
    for (const QString& address :
        QStringList{"https://www.nexusmods.com/games/monsterhunterwilds/mods/93",
            "/monsterhunterwilds/mods/93",
            "/games/monsterhunterwilds/mods/93",
            "//www.nexusmods.com/monsterhunterwilds/mods/93",
            "www.nexusmods.com/monsterhunterwilds/mods/93",
            "http://www.nexusmods.com/monsterhunterwilds/mods/93"})
    {
        expect(parseRequirementLink({{"url", address}, {"modId", "93"}}, "anothergame", "1")
                       .modKey() == "monsterhunterwilds/93",
            "Requirement URL variants preserve the correct game and mod");
    }
    expect(parseRequirementLink(
               {{"url", ""}, {"modId", 65}, {"gameId", 123}}, "monsterhunterwilds", "123")
                   .modKey() == "monsterhunterwilds/65",
        "Missing requirement URLs use explicit same-game identities");
    expect(!parseRequirementLink(
               {{"url", ""}, {"modId", 65}, {"gameId", 456}}, "monsterhunterwilds", "123")
               .valid(),
        "Cross-game requirements never inherit the parent game by guesswork");
    expect(!parseRequirementLink(
               {{"url", "https://evil.test/game/mods/93"}, {"modId", 93}}, "game", "123")
               .valid(),
        "Non-Nexus requirement links are rejected");
    expect(!parseModLink("https://nexusmods.com.evil.test/game/mods/1").valid(),
        "Lookalike hosts are rejected");
    expect(!parseModLink("https://evil.test@nexusmods.com/game/mods/1").valid(),
        "Embedded URL credentials are rejected");
    expect(
        !parseModLink("http://nexusmods.com/game/mods/1").valid(), "Unencrypted URLs are rejected");
    expect(!parseModLink("https://nexusmods.com:999/game/mods/1").valid(),
        "Unexpected ports are rejected");
    expect(!parseModLink("https://nexusmods.com/game/mods/0").valid(), "Invalid IDs are rejected");
    expect(!parseModLink("https://nexusmods.com/game/mods/1?file_id=../x").valid(),
        "File IDs cannot escape the path");
    expect(parseModLink("nxm://game/mods/1/files/2?key=x").file == "2", "NXM identity parses");
    const auto future = QString::number(QDateTime::currentSecsSinceEpoch() + 600);
    expect(validSignedNxm(QUrl("nxm://game/mods/1/files/2?key=x&expires=" + future)),
        "Authorized unexpired links are accepted");
    expect(!validSignedNxm(QUrl("nxm://game/mods/1/files/2?key=x&expires=1")),
        "Expired authorizations are rejected");
    expect(!validSignedNxm(QUrl("nxm://game/mods/1/files/2")), "Unsigned links cannot download");
    expect(!safeArchiveName("../../file.zip").contains('/'),
        "Archive names cannot traverse directories");
    expect(!safeArchiveName("C:\\file.zip").contains('\\'), "Windows separators are sanitized");
    expect(safeArchiveName("CON.zip") == "_CON.zip", "Windows device names are sanitized");
    expect(safeArchiveName("...") == "archive.bin", "Empty sanitized names have a safe fallback");
    expect(safeArchiveName("mod.zip. ") == "mod.zip", "Trailing dots and spaces are removed");
    expect(isDownloadHost(QUrl("https://cf-files.nexusmods.com/archive.zip")),
        "Official HTTPS archive hosts are accepted");
    expect(!isDownloadHost(QUrl("https://nexusmods.com.evil.test/archive.zip")),
        "Download host lookalikes are rejected");
    expect(!isDownloadHost(QUrl("http://files.nexusmods.com/archive.zip")),
        "Unencrypted archives are rejected");
    IntegrationTests::run();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
