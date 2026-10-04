#include "mainwindow.h"
#include "workflowhelpers.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSaveFile>
#include <QtWidgets>

namespace
{
QString defaultQueuePath()
{
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(directory);
    return directory + "/queue.json";
}
} // namespace

MainWindow::MainWindow(QNetworkAccessManager* transport, const QString& storagePath)
{
    m_openBrowser = [](const QUrl& url)
    {
        return QDesktopServices::openUrl(url);
    };
    m_networkManager = transport ? transport : new QNetworkAccessManager(this);
    m_queuePath = storagePath.isEmpty() ? defaultQueuePath() : storagePath;
    buildUi();
    restoreQueue();
    refreshQueue();
}

void MainWindow::log(const QString& message)
{
    m_activityLog->appendPlainText(QTime::currentTime().toString("HH:mm") + "  " + message);
}

std::shared_ptr<QueuedMod> MainWindow::selectedMod() const
{
    const int row = m_queueTable->currentRow();
    return row >= 0 && row < m_queue.size() ? m_queue[row] : nullptr;
}

void MainWindow::refreshQueue()
{
    const int row = m_queueTable->currentRow();
    m_queueTable->setRowCount(m_queue.size());
    int done = 0, ready = 0;
    for (int i = 0; i < m_queue.size(); ++i)
    {
        const auto& entry = m_queue[i];
        if (entry->downloaded)
        {
            ++done;
        }
        else if (entry->resolved && entry->state == "Ready to confirm")
        {
            ++ready;
        }
        QStringList values{entry->name + "\n" + entry->link.modKey(),
            entry->filename.isEmpty() ? "Not selected"
                                      : entry->filename + "\n" + nmd::formatFileSize(entry->size),
            entry->reason,
            entry->state};
        for (int col = 0; col < values.size(); ++col)
        {
            auto* item = m_queueTable->item(i, col);
            if (!item)
            {
                item = new QTableWidgetItem;
                m_queueTable->setItem(i, col, item);
            }
            item->setText(values[col]);
            item->setToolTip(entry->notes);
            if (col == 3)
            {
                item->setForeground(entry->downloaded                  ? QColor("#8bddbd")
                                    : entry->state.startsWith("Needs") ? QColor("#f0c785")
                                                                       : QColor("#b8c9dc"));
            }
        }
        m_queueTable->setRowHeight(i, 56);
    }
    if (row >= 0 && row < m_queue.size())
    {
        m_queueTable->selectRow(row);
    }
    m_queueSummary->setText(QString("QUEUE  ·  %1 mods / files  ·  %2 ready  ·  %3 saved")
            .arg(m_queue.size())
            .arg(ready)
            .arg(done));
    m_collectButton->setEnabled(!m_isResolving && !m_isDownloading);
    m_clearButton->setEnabled(!m_isResolving && !m_isDownloading);
    m_nextDownloadButton->setEnabled(!m_isResolving && ready > 0);
    m_stopButton->setEnabled(m_isResolving || m_isDownloading);
    m_apiKeyInput->setEnabled(!m_isResolving && !m_isDownloading);
    m_downloadFolderInput->setEnabled(!m_isResolving && !m_isDownloading);
}

void MainWindow::saveQueue()
{
    QJsonArray array;
    for (const auto& entry : m_queue)
    {
        array.append(QJsonObject{{"url", entry->link.page().toString()},
            {"name", entry->name},
            {"reason", entry->reason},
            {"notes", entry->notes},
            {"filename", entry->filename},
            {"version", entry->version},
            {"size", entry->size}});
    }
    QSaveFile file(m_queuePath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(
            QJsonObject{{"folder", m_downloadFolderInput->text()}, {"entries", array}})
                .toJson());
        if (!file.commit())
        {
            statusBar()->showMessage("Could not save the queue.");
        }
    }
}

void MainWindow::restoreQueue()
{
    QFile file(m_queuePath);
    if (file.open(QIODevice::ReadOnly))
    {
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        m_downloadFolderInput->setText(data["folder"].toString());
        for (const auto& value : data["entries"].toArray())
        {
            const auto object = value.toObject();
            auto entry = enqueueMod(parseModLink(object["url"].toString()),
                object["reason"].toString(),
                object["name"].toString(),
                object["notes"].toString());
            if (entry)
            {
                entry->filename = object["filename"].toString();
                entry->version = object["version"].toString();
                entry->size = object["size"].toInteger();
            }
        }
    }
    if (m_downloadFolderInput->text().isEmpty())
    {
        m_downloadFolderInput->setText(
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/Nexus Mods");
    }
    loadDownloadHistory();
}

void MainWindow::loadDownloadHistory()
{
    m_downloadHistory = {};
    QFile file(QDir(m_downloadFolderInput->text()).filePath(".nmd-downloads.json"));
    if (file.open(QIODevice::ReadOnly))
    {
        m_downloadHistory = QJsonDocument::fromJson(file.readAll()).object();
    }
    for (auto& entry : m_queue)
    {
        entry->downloaded = false;
        if (!hasCompletedDownload(entry))
        {
            entry->resolved = false;
            entry->state = "Waiting to check";
        }
    }
    refreshQueue();
}

void MainWindow::saveDownloadHistory()
{
    QSaveFile file(QDir(m_downloadFolderInput->text()).filePath(".nmd-downloads.json"));
    if (!file.open(QIODevice::WriteOnly))
    {
        log("Could not save download history: " + file.errorString());
        return;
    }
    file.write(QJsonDocument(m_downloadHistory).toJson());
    if (!file.commit())
    {
        log("Could not save download history: " + file.errorString());
    }
}

bool MainWindow::hasCompletedDownload(std::shared_ptr<QueuedMod> entry)
{
    if (entry->link.file.isEmpty())
    {
        return false;
    }
    const auto record = m_downloadHistory.value(nmd::downloadHistoryKey(*entry)).toObject();
    // Older history files do not have a version field. They remain compatible
    // and are validated by path and size below; new records also require the
    // same Nexus version before they can suppress a download.
    const auto recordedVersion = record["version"].toString();
    if (!recordedVersion.isEmpty() && recordedVersion != entry->version)
    {
        return false;
    }
    const auto expected = archiveRelativePath(entry->link, entry->filename);
    if (record["path"].toString() != expected)
    {
        return false;
    }
    const QFileInfo info(QDir(m_downloadFolderInput->text()).filePath(expected));
    if (info.isFile() && info.size() > 0 && info.size() == record["size"].toInteger())
    {
        entry->downloaded = true;
        entry->state = "Already saved";
        return true;
    }
    return false;
}

std::shared_ptr<QueuedMod> MainWindow::enqueueMod(
    const ModLink& link, const QString& reason, const QString& name, const QString& notes)
{
    if (!link.valid())
    {
        return nullptr;
    }
    for (auto& entry : m_queue)
    {
        if (entry->link.modKey() != link.modKey())
        {
            continue;
        }
        if (link.file.isEmpty() || entry->link.file.isEmpty() || link.file == entry->link.file)
        {
            if (entry->link.file.isEmpty() && !link.file.isEmpty())
            {
                entry->link.file = link.file;
                entry->resolved = false;
                entry->downloaded = false;
            }
            if (!notes.isEmpty() && !entry->notes.contains(notes))
            {
                entry->notes += "\n" + notes;
            }
            return entry;
        }
    }
    if (m_queue.size() >= 1000)
    {
        log("Queue limit reached (1,000 files). Split this batch into smaller lists.");
        return nullptr;
    }
    auto entry = std::make_shared<QueuedMod>();
    entry->link = link;
    entry->name = name.isEmpty() ? link.modKey() : name;
    entry->reason = reason;
    entry->notes = notes;
    m_queue.append(entry);
    return entry;
}

void MainWindow::stopWork()
{
    m_stopRequested = true;
    ++m_requestGeneration;
    m_authorizedLinks.clear();
    if (m_metadataReply)
    {
        m_metadataReply->abort();
    }
    if (m_downloadReply)
    {
        m_downloadReply->abort();
    }
    else
    {
        m_isDownloading = false;
    }
    m_isResolving = false;
    for (auto& entry : m_queue)
    {
        if (!entry->resolved)
        {
            entry->state = "Stopped";
        }
    }
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("Stopped");
    refreshQueue();
    saveQueue();
    log("Stopped. Completed archives are kept. Use Collect to retry checks, or confirm a download "
        "again on Nexus.");
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_isDownloading || m_isResolving)
    {
        if (QMessageBox::question(this,
                "Stop and close?",
                "Stop the current work and close? Completed downloads are kept; "
                "incomplete downloads will be discarded.") != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
        stopWork();
    }
    saveQueue();
    event->accept();
}
