#pragma once

#include "queuedmod.h"

#include <QJsonArray>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQueue>

#include <functional>
#include <memory>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QNetworkReply;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;

// Coordinates one queue. Metadata is resolved serially; archive transfers are also
// serial. Browser authorizations can arrive at any time and wait in memory.
class MainWindow : public QMainWindow
{
  public:
    explicit MainWindow(
        QNetworkAccessManager* transport = nullptr, const QString& storagePath = {});
    void acceptDownloadLink(const QString& text);

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    friend class IntegrationTests;
    using JsonResultHandler = std::function<void(QJsonDocument, QString)>;

    // Interface and persisted queue.
    void buildUi();
    void refreshQueue();
    void log(const QString& message);
    void saveQueue();
    void restoreQueue();
    std::shared_ptr<QueuedMod> selectedMod() const;
    std::shared_ptr<QueuedMod> enqueueMod(const ModLink& link,
        const QString& reason,
        const QString& name = {},
        const QString& notes = {});
    void openModPage(bool advance);
    void configureDownloadHandler();
    void stopWork();

    // API access. The generation counter invalidates callbacks after Stop.
    void requestJson(const QString& path,
        JsonResultHandler callback,
        const QJsonObject& body = {},
        int retry = 0);
    void validateAccount(std::function<void()> callback);

    // Dependency traversal: mod metadata -> requirements -> selected file.
    void collectMods();
    void resolveNextMod();
    void resolveMod(std::shared_ptr<QueuedMod> entry);
    void resolveModRequirements(std::shared_ptr<QueuedMod> entry, int offset = 0);
    void selectModFile(std::shared_ptr<QueuedMod> entry, bool legacy);
    void resolveFileDependencies(std::shared_ptr<QueuedMod> entry);
    void selectDependencyCandidates(
        std::shared_ptr<QueuedMod> entry, const QJsonArray& definitions, int index = 0);
    void finishModResolution(std::shared_ptr<QueuedMod> entry, const QString& error = {});

    // User-authorized downloads and completion records.
    void processAuthorizedDownloads();
    void downloadArchive(std::shared_ptr<QueuedMod> entry, const QUrl& url);
    bool hasCompletedDownload(std::shared_ptr<QueuedMod> entry);
    void loadDownloadHistory();
    void saveDownloadHistory();

    QLineEdit* m_apiKeyInput = nullptr;
    QLineEdit* m_downloadFolderInput = nullptr;
    QPlainTextEdit* m_modLinksInput = nullptr;
    QPlainTextEdit* m_activityLog = nullptr;
    QTableWidget* m_queueTable = nullptr;
    QLabel* m_queueSummary = nullptr;
    QLabel* m_accountStatus = nullptr;
    QPushButton* m_collectButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_nextDownloadButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QProgressBar* m_progressBar = nullptr;

    QNetworkAccessManager* m_networkManager = nullptr;
    QPointer<QNetworkReply> m_metadataReply;
    QPointer<QNetworkReply> m_downloadReply;

    QString m_queuePath;
    std::function<bool(const QUrl&)> m_openBrowser;
    QList<std::shared_ptr<QueuedMod>> m_queue;
    QJsonObject m_downloadHistory;

    // Credentials and signed URLs intentionally never enter persisted state.
    QQueue<QString> m_authorizedLinks;
    QString m_userId;
    QString m_sessionApiKey;

    bool m_isResolving = false;
    bool m_isDownloading = false;
    bool m_stopRequested = false;
    int m_requestGeneration = 0;
};
