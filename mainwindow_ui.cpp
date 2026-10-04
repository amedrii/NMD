#include "mainwindow.h"
#include "workflowhelpers.h"
#include <QtWidgets>

void MainWindow::buildUi()
{
    setWindowTitle("NMD · Nexus Mod Downloader");
    resize(1120, 940);
    setMinimumSize(900, 820);
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(28, 24, 28, 20);
    layout->setSpacing(14);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel("NMD  /  YOUR MOD QUEUE");
    title->setObjectName("title");
    auto* badge = new QLabel("FREE ACCOUNT · CONFIRM ON NEXUS");
    badge->setObjectName("badge");
    header->addWidget(title);
    header->addStretch();
    header->addWidget(badge);
    layout->addLayout(header);
    auto* intro = new QLabel(
        "Paste your mods. Collect their requirements. Keep every download in one place.");
    intro->setObjectName("intro");
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* settings = new QGroupBox("01   CONNECT AND CHOOSE A FOLDER");
    settings->setMinimumHeight(220);
    auto* form = new QGridLayout(settings);
    m_apiKeyInput = new QLineEdit;
    m_apiKeyInput->setEchoMode(QLineEdit::Password);
    m_apiKeyInput->setPlaceholderText("Personal Nexus API key · kept only for this session");
    m_apiKeyInput->setText(qEnvironmentVariable("NEXUS_API_KEY"));
    auto* keyPage = new QPushButton("Get API key");
    form->addWidget(new QLabel("API key"), 0, 0);
    form->addWidget(m_apiKeyInput, 0, 1);
    form->addWidget(keyPage, 0, 2);
    m_downloadFolderInput = new QLineEdit;
    m_downloadFolderInput->setPlaceholderText("Choose where to save mod archives");
    auto* browse = new QPushButton("Choose folder");
    form->addWidget(new QLabel("Save to"), 1, 0);
    form->addWidget(m_downloadFolderInput, 1, 1);
    form->addWidget(browse, 1, 2);
    auto* protocol = new QPushButton("Connect Nexus download buttons");
    auto* pasteNxm = new QPushButton("Paste download link");
    auto* tools = new QHBoxLayout;
    tools->addWidget(protocol);
    tools->addWidget(pasteNxm);
    tools->addStretch();
    m_accountStatus =
        new QLabel("Confirm each file using “Mod manager download” → “Slow download” on Nexus.");
    m_accountStatus->setWordWrap(true);
    form->addLayout(tools, 2, 0, 1, 3);
    form->addWidget(m_accountStatus, 3, 0, 1, 3);
    layout->addWidget(settings);

    auto* inputBox = new QGroupBox("02   ADD YOUR MODS");
    auto* inputLayout = new QVBoxLayout(inputBox);
    m_modLinksInput = new QPlainTextEdit;
    m_modLinksInput->setPlaceholderText(
        "https://www.nexusmods.com/skyrimspecialedition/mods/266\nhttps://www.nexusmods.com/…\nOne "
        "mod link per line. Shared requirements are added only once.");
    m_modLinksInput->setMaximumHeight(105);
    inputLayout->addWidget(m_modLinksInput);
    auto* actions = new QHBoxLayout;
    m_collectButton = new QPushButton("Collect mods + requirements");
    m_collectButton->setObjectName("primary");
    m_stopButton = new QPushButton("Stop");
    m_stopButton->setEnabled(false);
    m_clearButton = new QPushButton("Clear queue");
    actions->addWidget(m_collectButton);
    actions->addWidget(m_stopButton);
    actions->addStretch();
    actions->addWidget(m_clearButton);
    inputLayout->addLayout(actions);
    layout->addWidget(inputBox);

    auto* queueHeader = new QHBoxLayout;
    m_queueSummary = new QLabel("YOUR QUEUE");
    auto* open = new QPushButton("Open selected on Nexus");
    m_nextDownloadButton = new QPushButton("Open next download");
    m_nextDownloadButton->setObjectName("primary");
    queueHeader->addWidget(m_queueSummary);
    queueHeader->addStretch();
    queueHeader->addWidget(open);
    queueHeader->addWidget(m_nextDownloadButton);
    layout->addLayout(queueHeader);
    m_queueTable = new QTableWidget(0, 4);
    m_queueTable->setHorizontalHeaderLabels({"Mod / game", "Selected file", "Added for", "Status"});
    m_queueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_queueTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_queueTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_queueTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_queueTable->verticalHeader()->hide();
    m_queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_queueTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_queueTable->setAlternatingRowColors(true);
    m_queueTable->setMinimumHeight(150);
    layout->addWidget(m_queueTable, 1);
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("Ready");
    layout->addWidget(m_progressBar);
    m_activityLog = new QPlainTextEdit;
    m_activityLog->setReadOnly(true);
    m_activityLog->setMaximumHeight(115);
    m_activityLog->setPlaceholderText(
        "Requirement notes, off-site dependencies and download results appear here.");
    layout->addWidget(m_activityLog);

    setStyleSheet(R"(
        QWidget { background: #14191f; color: #e7edf4; font-family: "Segoe UI"; font-size: 13px; }
        QLabel#title { font-size: 22px; font-weight: 700; letter-spacing: 1px; }
        QLabel#intro { color: #acb9c8; font-size: 14px; }
        QLabel#badge { color: #81d8c7; background: #203630; padding: 8px 12px; border-radius: 5px; font-size: 11px; }
        QGroupBox { border: 1px solid #303c49; border-radius: 8px; margin-top: 10px; padding: 16px 12px 10px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #8facc8; }
        QLineEdit, QPlainTextEdit { background: #0e1319; border: 1px solid #344151; border-radius: 5px; padding: 8px; selection-background-color: #315d72; }
        QLineEdit { min-height: 20px; }
        QLineEdit:focus, QPlainTextEdit:focus { border-color: #7ccebb; }
        QPushButton { min-height: 18px; background: #273340; border: 1px solid #3a495b; border-radius: 5px; padding: 9px 14px; font-weight: 600; }
        QPushButton:hover { background: #34465a; }
        QPushButton#primary { background: #8bddbd; color: #11281f; border-color: #8bddbd; }
        QPushButton#primary:hover { background: #a5eed2; }
        QPushButton:disabled { background: #202832; color: #657180; border-color: #303a45; }
        QTableWidget { background: #10161c; alternate-background-color: #192129; border: 1px solid #303c49; border-radius: 6px; gridline-color: #273340; }
        QTableWidget::item { padding: 7px; }
        QTableWidget::item:selected { background: #274653; color: #ffffff; }
        QHeaderView::section { background: #202a35; padding: 9px; border: 0; color: #a6bbcf; font-weight: 600; }
        QProgressBar { border: 1px solid #344151; border-radius: 4px; text-align: center; background: #10161c; height: 20px; }
        QProgressBar::chunk { background: #366e60; }
        QToolTip { background: #273340; color: #ffffff; border: 1px solid #526274; padding: 5px; }
    )");
    connect(keyPage, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(
            QUrl("https://www.nexusmods.com/users/myaccount?tab=api%20access"));
    });
    connect(browse,
        &QPushButton::clicked,
        this,
        [this]
    {
        if (m_isResolving || m_isDownloading)
        {
            return;
        }
        const auto dir = QFileDialog::getExistingDirectory(
            this, "Save mod archives to", m_downloadFolderInput->text());
        if (!dir.isEmpty())
        {
            m_downloadFolderInput->setText(dir);
            loadDownloadHistory();
            saveQueue();
        }
    });
    connect(m_downloadFolderInput,
        &QLineEdit::editingFinished,
        this,
        [this]
    {
        loadDownloadHistory();
        saveQueue();
    });
    connect(m_collectButton, &QPushButton::clicked, this, &MainWindow::collectMods);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopWork);
    connect(protocol, &QPushButton::clicked, this, &MainWindow::configureDownloadHandler);
    connect(pasteNxm,
        &QPushButton::clicked,
        this,
        [this]
    {
        bool ok;
        const auto text = QInputDialog::getText(this,
            "Authorized download link",
            "Paste the nxm:// link supplied by Nexus:",
            QLineEdit::Normal,
            {},
            &ok);
        if (ok && !text.trimmed().isEmpty())
        {
            acceptDownloadLink(text);
        }
    });
    connect(open, &QPushButton::clicked, this, [this] { openModPage(false); });
    connect(m_nextDownloadButton, &QPushButton::clicked, this, [this] { openModPage(true); });
    connect(m_clearButton,
        &QPushButton::clicked,
        this,
        [this]
    {
        m_queue.clear();
        m_authorizedLinks.clear();
        m_activityLog->clear();
        refreshQueue();
        saveQueue();
    });
    connect(m_queueTable, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem*) {
        openModPage(false);
    });
    connect(m_queueTable,
        &QTableWidget::itemSelectionChanged,
        this,
        [this]
    {
        if (auto entry = selectedMod())
        {
            statusBar()->showMessage(
                entry->notes.isEmpty() ? entry->link.page().toString() : entry->notes);
        }
    });
}

void MainWindow::openModPage(bool advance)
{
    if (m_isResolving)
    {
        return;
    }
    auto entry = selectedMod();
    if (advance)
    {
        entry.reset();
        for (const auto& candidate : m_queue)
        {
            if (candidate->resolved && !candidate->downloaded &&
                candidate->state == "Ready to confirm")
            {
                entry = candidate;
                break;
            }
        }
    }
    if (!entry)
    {
        log(advance ? "No downloads are ready. Reopen a waiting mod with Open selected on Nexus, "
                      "or collect requirements again."
                    : "Select a mod in the queue first.");
        return;
    }
    if (!m_openBrowser(entry->link.page()))
    {
        log("Could not open your browser.");
        return;
    }
    if (!entry->downloaded && !entry->state.startsWith("Needs"))
    {
        entry->state = "Waiting for confirmation";
    }
    log(entry->name + " — choose file ID " + entry->link.file +
        " on Nexus, then Mod manager download → Slow download.");
    m_queueTable->selectRow(m_queue.indexOf(entry));
    refreshQueue();
}

void MainWindow::configureDownloadHandler()
{
#ifdef Q_OS_WIN
    QSettings registry("HKEY_CURRENT_USER\\Software\\Classes\\nxm", QSettings::NativeFormat);
    const QString command =
        "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\" \"%1\"";
    const QString previous = registry.value("shell/open/command/.").toString();
    QSettings settings;
    if (previous == command)
    {
        if (QMessageBox::question(this,
                "Disconnect download buttons",
                "Restore the previous Nexus download handler?") != QMessageBox::Yes)
        {
            return;
        }
        const QString old = settings.value("previousNxmCommand").toString();
        if (!old.isEmpty())
        {
            registry.setValue("shell/open/command/.", old);
        }
        else
        {
            registry.remove("");
        }
        registry.sync();
        log("Previous Nexus download handler restored.");
        return;
    }
    QString text = "Let Nexus “Mod manager download” buttons open this app?\n\nThis changes the "
                   "nxm link handler for your Windows account. ";
    if (!previous.isEmpty())
    {
        text += "Your current mod manager will stop receiving these links. ";
    }
    text += "Click this button again to restore the previous handler.";
    if (QMessageBox::question(this, "Connect Nexus download buttons", text) != QMessageBox::Yes)
    {
        return;
    }
    settings.setValue("previousNxmCommand", previous);
    registry.setValue(".", "URL:Nexus Mod Download");
    registry.setValue("URL Protocol", "");
    registry.setValue("shell/open/command/.", command);
    registry.sync();
    if (registry.status() != QSettings::NoError)
    {
        log("Could not register the Nexus link handler.");
    }
    else
    {
        log("Connected. Keep this app in its current location. Your browser may ask to allow NMD "
            "to open links.");
    }
#else
    log("Automatic nxm registration is available on Windows. You can paste an authorized nxm link "
        "instead.");
#endif
}
