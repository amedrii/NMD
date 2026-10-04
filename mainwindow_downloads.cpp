#include "mainwindow.h"
#include "workflowhelpers.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSaveFile>
#include <QUrlQuery>
#include <QtWidgets>

void MainWindow::acceptDownloadLink(const QString& text)
{
    QString error;
    if (!validSignedNxm(QUrl(text), &error))
    {
        log(error);
        return;
    }
    if (!m_authorizedLinks.contains(text))
    {
        m_authorizedLinks.enqueue(text);
    }
    log("Received an authorized download link.");
    processAuthorizedDownloads();
}

void MainWindow::processAuthorizedDownloads()
{
    if (m_isResolving || m_isDownloading || m_authorizedLinks.isEmpty())
    {
        return;
    }
    QString error;
    const QString signedText = m_authorizedLinks.head();
    const QUrl nxm(signedText);
    if (!validSignedNxm(nxm, &error))
    {
        log(error);
        m_authorizedLinks.dequeue();
        QTimer::singleShot(0, this, &MainWindow::processAuthorizedDownloads);
        return;
    }
    if (!QDir::isAbsolutePath(m_downloadFolderInput->text()) ||
        m_downloadFolderInput->text().trimmed().isEmpty())
    {
        log("Choose an absolute download folder before receiving downloads.");
        return;
    }
    const auto link = parseModLink(signedText);
    auto entry = enqueueMod(link, "Nexus download");
    if (!entry)
    {
        m_authorizedLinks.dequeue();
        return;
    }
    if (!entry->resolved)
    {
        m_isResolving = true;
        m_stopRequested = false;
        ++m_requestGeneration;
        refreshQueue();
        validateAccount([this] { resolveNextMod(); });
        return;
    }
    if (entry->state.startsWith("Needs"))
    {
        log(entry->name +
            " — fix its requirement or file check first, then confirm on Nexus again.");
        m_authorizedLinks.dequeue();
        QTimer::singleShot(0, this, &MainWindow::processAuthorizedDownloads);
        return;
    }
    if (hasCompletedDownload(entry))
    {
        log(entry->name + " — already saved; duplicate download skipped.");
        m_authorizedLinks.dequeue();
        refreshQueue();
        QTimer::singleShot(0, this, &MainWindow::processAuthorizedDownloads);
        return;
    }
    m_isDownloading = true;
    m_stopRequested = false;
    refreshQueue();
    validateAccount([this, entry, nxm]
    {
        const QUrlQuery auth(nxm);
        const auto linkUser = auth.queryItemValue("user_id");
        if (!linkUser.isEmpty() && linkUser != m_userId)
        {
            log("This download link belongs to a different Nexus account than your API key.");
            m_authorizedLinks.dequeue();
            m_isDownloading = false;
            refreshQueue();
            processAuthorizedDownloads();
            return;
        }
        QUrlQuery query;
        query.addQueryItem("key",
            QString::fromLatin1(
                QUrl::toPercentEncoding(auth.queryItemValue("key", QUrl::FullyDecoded))));
        query.addQueryItem("expires", auth.queryItemValue("expires"));
        entry->state = "Authorizing download";
        refreshQueue();
        requestJson(nmd::modApiPath(*entry) + "/files/" + entry->link.file +
                        "/download_link.json?" + query.toString(QUrl::FullyEncoded),
            [this, entry](QJsonDocument document, QString requestError)
        {
            m_authorizedLinks.dequeue();
            if (!requestError.isEmpty())
            {
                entry->state = "Ready to confirm";
                log(requestError);
                m_isDownloading = false;
                refreshQueue();
                processAuthorizedDownloads();
                return;
            }
            QUrl url;
            for (const auto& value : document.array())
            {
                const QUrl candidate(value.toObject()["URI"].toString());
                if (isDownloadHost(candidate))
                {
                    url = candidate;
                    break;
                }
            }
            if (url.isEmpty())
            {
                entry->state = "Needs attention";
                log("Nexus returned no supported HTTPS download mirror.");
                m_isDownloading = false;
                refreshQueue();
                processAuthorizedDownloads();
                return;
            }
            downloadArchive(entry, url);
        });
    });
}

void MainWindow::downloadArchive(std::shared_ptr<QueuedMod> entry, const QUrl& initialUrl)
{
    const QString relative = archiveRelativePath(entry->link, entry->filename);
    const QString target = QDir(m_downloadFolderInput->text()).filePath(relative);
    auto failEarly = [this, entry](const QString& message)
    {
        entry->state = "Ready to confirm";
        log(entry->name + " — " + message);
        m_isDownloading = false;
        refreshQueue();
        processAuthorizedDownloads();
    };
    if (QFileInfo::exists(target))
    {
        failEarly("A file already exists at this path, but it is not a recorded completed NMD "
                  "download. Move it or choose another folder; it was not overwritten.");
        return;
    }
    if (!QDir().mkpath(QFileInfo(target).absolutePath()))
    {
        failEarly("Cannot create the download folder.");
        return;
    }
    struct Transfer
    {
        std::shared_ptr<QSaveFile> output;
        QCryptographicHash hash{QCryptographicHash::Sha256};
        qint64 bytes = 0;
        int redirects = 0;
        QString error;
    };
    auto transfer = std::make_shared<Transfer>();
    transfer->output = std::make_shared<QSaveFile>(target);
    transfer->output->setDirectWriteFallback(false);
    if (!transfer->output->open(QIODevice::WriteOnly))
    {
        failEarly(transfer->output->errorString());
        return;
    }
    entry->state = "Downloading";
    refreshQueue();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    // Each request owns its callback. The recursive function is copied, never retained by itself.
    using Start = std::function<void(const QUrl&)>;
    auto start = std::make_shared<Start>();
    std::weak_ptr<Start> weak = start;
    *start = [this, entry, transfer, target, relative, weak](const QUrl& url)
    {
        auto keepAlive = weak.lock();
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", "NMD/0.1.0");
        request.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(60000);
        auto* reply = m_networkManager->get(request);
        m_downloadReply = reply;
        auto drain = [reply, transfer]
        {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status != 200)
            {
                reply->readAll();
                return;
            }
            const auto bytes = reply->readAll();
            if (bytes.isEmpty() || !transfer->error.isEmpty())
            {
                return;
            }
            if (transfer->output->write(bytes) != bytes.size())
            {
                transfer->error = "Cannot write to the destination (check free disk space).";
                reply->abort();
                return;
            }
            transfer->hash.addData(bytes);
            transfer->bytes += bytes.size();
        };
        connect(reply, &QNetworkReply::readyRead, this, drain);
        connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            [this, entry](qint64 received, qint64 total)
        {
            const qint64 expected = total > 0 ? total : entry->size;
            m_progressBar->setRange(0, expected > 0 ? 100 : 0);
            if (expected > 0)
            {
                m_progressBar->setValue(int(qMin(100.0, 100.0 * received / expected)));
            }
            m_progressBar->setFormat(QString("%1 · %2 / %3")
                    .arg(
                        entry->name, nmd::formatFileSize(received), nmd::formatFileSize(expected)));
        });
        connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, drain, entry, transfer, target, relative, keepAlive]
        {
            drain();
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status >= 300 && status < 400 && !m_stopRequested)
            {
                const QUrl redirect = reply->url().resolved(
                    reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
                if (++transfer->redirects <= 5 && isDownloadHost(redirect))
                {
                    (*keepAlive)(redirect);
                    return;
                }
                transfer->error = "Download redirected to an unsupported destination.";
            }
            if (m_stopRequested)
            {
                transfer->error = "Download stopped. Confirm it again on Nexus to retry.";
            }
            else if (transfer->error.isEmpty() &&
                     (reply->error() != QNetworkReply::NoError || status != 200))
            {
                transfer->error =
                    QString("Download failed (HTTP %1). Confirm it again on Nexus to retry.")
                        .arg(status);
            }
            const qint64 length = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            if (transfer->error.isEmpty() &&
                (transfer->bytes == 0 || (length > 0 && length != transfer->bytes) ||
                    (entry->size > 0 && entry->size != transfer->bytes)))
            {
                transfer->error =
                    "The archive size does not match. The incomplete download was discarded.";
            }
            const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
            if (transfer->error.isEmpty() &&
                (contentType.startsWith("text/") || contentType.contains("json")))
            {
                transfer->error = "Nexus returned a page instead of an archive.";
            }
            if (transfer->error.isEmpty() && QFileInfo::exists(target))
            {
                transfer->error =
                    "Another file appeared at the destination. It was not overwritten.";
            }
            if (transfer->error.isEmpty() && !transfer->output->commit())
            {
                transfer->error =
                    "Could not finalize the archive: " + transfer->output->errorString();
            }
            if (!transfer->error.isEmpty())
            {
                transfer->output->cancelWriting();
                entry->state = "Ready to confirm";
                log(entry->name + " — " + transfer->error);
            }
            else
            {
                m_downloadHistory[nmd::downloadHistoryKey(*entry)] = QJsonObject{{"path", relative},
                    {"size", transfer->bytes},
                    {"sha256", QString::fromLatin1(transfer->hash.result().toHex())},
                    {"version", entry->version},
                    {"completedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
                saveDownloadHistory();
                entry->downloaded = true;
                entry->state = "Saved";
                log("Saved " + entry->name + " → " + target);
            }
            m_isDownloading = false;
            m_downloadReply = nullptr;
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(entry->downloaded ? 100 : 0);
            m_progressBar->setFormat(
                entry->downloaded ? "Download saved" : "Download not completed");
            refreshQueue();
            saveQueue();
            processAuthorizedDownloads();
        });
    };
    (*start)(initialUrl);
}
