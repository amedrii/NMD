#include "mainwindow.h"
#include "workflowhelpers.h"
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProgressBar>
#include <QTimer>

void MainWindow::requestJson(
    const QString& path, JsonResultHandler callback, const QJsonObject& body, int retry)
{
    const int token = m_requestGeneration;
    QNetworkRequest request(QUrl("https://api.nexusmods.com" + path));
    request.setRawHeader("apikey", m_sessionApiKey.toUtf8());
    request.setRawHeader("Application-Name", "NMD");
        request.setRawHeader("Application-Version", "0.1.1p");
        request.setRawHeader("User-Agent", "NMD/0.1.1p");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    QNetworkReply* reply;
    if (body.isEmpty())
    {
        reply = m_networkManager->get(request);
    }
    else
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = m_networkManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    }
    m_metadataReply = reply;
    connect(reply,
        &QNetworkReply::finished,
        this,
        [this, reply, path, callback, body, retry, token]
    {
        reply->deleteLater();
        if (token != m_requestGeneration)
        {
            return;
        }
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if ((code == 429 || code == 502 || code == 503) && retry < 3)
        {
            const int seconds = qBound(2,
                reply->rawHeader("Retry-After").toInt() > 0
                    ? reply->rawHeader("Retry-After").toInt()
                    : (retry + 1) * 3,
                60);
            log(QString("Nexus is busy or rate limiting. Retrying in %1 seconds.").arg(seconds));
            QTimer::singleShot(seconds * 1000,
                this,
                [this, token, path, callback, body, retry]
            {
                if (m_requestGeneration == token)
                {
                    requestJson(path, callback, body, retry + 1);
                }
            });
            return;
        }
        if (reply->error() != QNetworkReply::NoError || code < 200 || code >= 300)
        {
            QString error = QString("Nexus request failed (HTTP %1).").arg(code);
            if (code == 401)
            {
                error = "API key rejected. Generate a personal API key and try again.";
            }
            else if (code == 403)
            {
                error = "Nexus denied access. Check your API key, account permissions, or "
                        "reconfirm an expired download.";
            }
            else if (code == 404)
            {
                error = "The mod or file is unavailable on Nexus.";
            }
            else if (code == 429)
            {
                error = "Nexus API limit reached. Try again later.";
            }
            else if (code == 0)
            {
                error = "Cannot reach Nexus. Check your connection and try again.";
            }
            callback({}, error);
            return;
        }
        QJsonParseError parse;
        auto document = QJsonDocument::fromJson(reply->readAll(), &parse);
        if (parse.error != QJsonParseError::NoError || document.isNull())
        {
            callback({}, "Nexus returned an unreadable response.");
            return;
        }
        if (!document.object()["errors"].toArray().isEmpty())
        {
            callback({},
                "The Nexus requirement API could not answer this query. Review this "
                "mod on Nexus.");
            return;
        }
        callback(document, {});
    });
}

void MainWindow::validateAccount(std::function<void()> callback)
{
    m_sessionApiKey = m_apiKeyInput->text().trimmed();
    if (m_sessionApiKey.isEmpty())
    {
        m_isResolving = false;
        m_isDownloading = false;
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_progressBar->setFormat("Check your API key or connection");
        refreshQueue();
        QMessageBox::information(this,
            "API key needed",
            "Add your personal Nexus API key. Use Get API key to open your "
            "account settings. Free accounts can use it.");
        return;
    }
    requestJson("/v1/users/validate.json",
        [this, callback](QJsonDocument document, QString error)
    {
        if (!error.isEmpty())
        {
            log(error);
            m_isResolving = false;
            m_isDownloading = false;
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(0);
            m_progressBar->setFormat("Check your API key or connection");
            refreshQueue();
            return;
        }
        m_userId = jsonId(document.object()["user_id"]);
        m_accountStatus->setText("Connected as " + document.object()["name"].toString() +
                                 " · Confirm each file on Nexus to start its download.");
        callback();
    });
}
