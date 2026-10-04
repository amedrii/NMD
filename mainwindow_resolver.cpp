#include "mainwindow.h"
#include "workflowhelpers.h"
#include <QJsonDocument>
#include <QtWidgets>

void MainWindow::collectMods()
{
    if (m_downloadFolderInput->text().trimmed().isEmpty() ||
        !QDir::isAbsolutePath(m_downloadFolderInput->text()))
    {
        QMessageBox::information(this, "Download folder", "Choose an absolute folder path first.");
        return;
    }
    QStringList invalid;
    for (const auto& line : m_modLinksInput->toPlainText().split('\n'))
    {
        if (line.trimmed().isEmpty())
        {
            continue;
        }
        auto link = parseModLink(line);
        if (!link.valid())
        {
            invalid << line.left(120);
        }
        else
        {
            enqueueMod(link, "Your list");
        }
    }
    if (!invalid.isEmpty())
    {
        log(QString("Ignored %1 invalid link(s). Use https://www.nexusmods.com/GAME/mods/ID.")
                .arg(invalid.size()));
    }
    if (m_queue.isEmpty())
    {
        log("Paste at least one Nexus mod link.");
        return;
    }
    // A repeated run retries failures; completed requirement traversals remain cached in this
    // session.
    for (auto& entry : m_queue)
    {
        if (entry->state.startsWith("Needs") || entry->state == "Stopped")
        {
            entry->resolved = false;
        }
    }
    m_isResolving = true;
    m_stopRequested = false;
    ++m_requestGeneration;
    refreshQueue();
    m_progressBar->setRange(0, 0);
    m_progressBar->setFormat("Collecting requirements…");
    validateAccount([this] { resolveNextMod(); });
}

void MainWindow::resolveNextMod()
{
    if (!m_isResolving)
    {
        return;
    }
    for (auto& entry : m_queue)
    {
        if (!entry->resolved)
        {
            resolveMod(entry);
            return;
        }
    }
    m_isResolving = false;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("Queue checked");
    log("Queue checked. Review any requirement notes, then use Open next download.");
    refreshQueue();
    saveQueue();
    processAuthorizedDownloads();
}

void MainWindow::resolveMod(std::shared_ptr<QueuedMod> entry)
{
    entry->state = "Checking requirements";
    refreshQueue();
    requestJson(nmd::modApiPath(*entry) + ".json",
        [this, entry](QJsonDocument document, QString error)
    {
        if (!error.isEmpty())
        {
            finishModResolution(entry, error);
            return;
        }
        auto object = document.object();
        if (!object.contains("game_id") || object["name"].toString().isEmpty())
        {
            finishModResolution(entry, "Missing mod details from Nexus.");
            return;
        }
        entry->name = object["name"].toString();
        entry->gameId = jsonId(object["game_id"]);
        resolveModRequirements(entry);
    });
}

void MainWindow::resolveModRequirements(std::shared_ptr<QueuedMod> entry, int offset)
{
    const QString query = R"(query Requirements($mod: ID!, $game: ID!, $offset: Int!) {
        mod(modId: $mod, gameId: $game) {
            legacyModRequirementsEnabled
            modRequirements {
                nexusRequirements(offset: $offset, count: 100) {
                    totalCount nodes { externalRequirement gameId modId modName notes url }
                }
                dlcRequirements { notes gameExpansion { name } }
            }
        }
    })";
    QJsonObject variables{{"mod", entry->link.mod}, {"game", entry->gameId}, {"offset", offset}};
    requestJson("/v2/graphql",
        [this, entry, offset](QJsonDocument document, QString error)
    {
        if (!error.isEmpty())
        {
            finishModResolution(entry, error);
            return;
        }
        const auto mod = document.object()["data"].toObject()["mod"].toObject();
        if (!mod.contains("legacyModRequirementsEnabled") || !mod["modRequirements"].isObject())
        {
            finishModResolution(
                entry, "Nexus did not provide requirement metadata. Review the Requirements tab.");
            return;
        }
        const auto requirements = mod["modRequirements"].toObject();
        const bool legacy = mod["legacyModRequirementsEnabled"].toBool();
        if (offset == 0)
        {
            for (const auto& value : requirements["dlcRequirements"].toArray())
            {
                auto dlc = value.toObject();
                const auto note =
                    "DLC needed: " + dlc["gameExpansion"].toObject()["name"].toString() + " " +
                    dlc["notes"].toString();
                entry->notes += "\n" + note;
                log(entry->name + " — " + note);
            }
        }
        const auto page = requirements["nexusRequirements"].toObject();
        if (!page.contains("nodes") || !page.contains("totalCount"))
        {
            finishModResolution(
                entry, "Nexus returned incomplete requirements. Review this mod on Nexus.");
            return;
        }
        const auto nodes = page["nodes"].toArray();
        for (const auto& value : nodes)
        {
            const auto dep = value.toObject();
            const QString name = dep["modName"].toString(), notes = dep["notes"].toString();
            if (dep["externalRequirement"].toBool())
            {
                const QString note =
                    "Off-site requirement: " + name + " — " + dep["url"].toString() + " " + notes;
                entry->notes += "\n" + note;
                log(entry->name + " — " + note);
            }
            else if (legacy)
            {
                auto link = parseRequirementLink(dep, entry->link.game, entry->gameId);
                if (!link.valid())
                {
                    finishModResolution(entry,
                        "A requirement URL could not be read: " + name + ". Check it on Nexus.");
                    return;
                }
                if (!enqueueMod(link, entry->name, name, notes))
                {
                    finishModResolution(entry,
                        "Could not add all requirements. Split the queue into "
                        "smaller batches.");
                    return;
                }
                if (!notes.isEmpty())
                {
                    log(name + " — author note: " + notes);
                }
            }
        }
        const int total = page["totalCount"].toInt();
        if (offset + nodes.size() < total)
        {
            if (nodes.isEmpty() || offset >= 10000)
            {
                finishModResolution(entry, "Incomplete requirement pages.");
                return;
            }
            resolveModRequirements(entry, offset + nodes.size());
            return;
        }
        selectModFile(entry, legacy);
    },
        {{"query", query}, {"variables", variables}});
}

void MainWindow::selectModFile(std::shared_ptr<QueuedMod> entry, bool legacy)
{
    requestJson(nmd::modApiPath(*entry) + "/files.json",
        [this, entry, legacy](QJsonDocument document, QString error)
    {
        if (!error.isEmpty())
        {
            finishModResolution(entry, error);
            return;
        }
        const auto files = document.object()["files"].toArray();
        QJsonObject chosen;
        QList<QJsonObject> available, main;
        for (const auto& value : files)
        {
            const auto file = value.toObject();
            const int category = file["category_id"].toInt();
            if (!entry->link.file.isEmpty() && jsonId(file["file_id"]) == entry->link.file)
            {
                chosen = file;
            }
            if (category >= 1 && category <= 4)
            {
                available << file;
                if (category == 1)
                {
                    main << file;
                }
            }
        }
        if (!entry->link.file.isEmpty() && chosen.isEmpty())
        {
            finishModResolution(entry,
                "The requested file is unavailable. Open this mod on Nexus to "
                "review its files.");
            return;
        }
        if (entry->link.file.isEmpty())
        {
            const bool isUserSelectedMod = entry->reason == "Your list";
            if (main.size() == 1 && (!isUserSelectedMod || available.size() == 1))
            {
                chosen = main.first();
            }
            else if (isUserSelectedMod && available.size() > 1)
            {
                QDialog dialog(this);
                dialog.setWindowTitle("Choose files · " + entry->name);
                dialog.resize(700, 430);
                auto* layout = new QVBoxLayout(&dialog);
                layout->addWidget(
                    new QLabel("Select every file you want downloaded for this mod. "
                               "For example, choose the main archive and its texture archive.",
                        &dialog));
                auto* fileList = new QListWidget(&dialog);
                fileList->setSelectionMode(QAbstractItemView::MultiSelection);
                for (const auto& file : available)
                {
                    auto* item = new QListWidgetItem(
                        file["name"].toString() + " · " + file["version"].toString() + " · " +
                            file["category_name"].toString() + " · ID " + jsonId(file["file_id"]),
                        fileList);
                    item->setData(Qt::UserRole, QJsonDocument(file).toJson(QJsonDocument::Compact));
                    if (file["category_id"].toInt() == 1)
                    {
                        item->setSelected(true);
                    }
                }
                layout->addWidget(fileList, 1);
                auto* buttons =
                    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                layout->addWidget(buttons);
                QObject::connect(buttons,
                    &QDialogButtonBox::accepted,
                    &dialog,
                    [&]
                {
                    if (fileList->selectedItems().isEmpty())
                    {
                        QMessageBox::information(
                            &dialog, "Choose a file", "Select at least one file to continue.");
                        return;
                    }
                    dialog.accept();
                });
                QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                if (dialog.exec() != QDialog::Accepted)
                {
                    finishModResolution(
                        entry, "File selection postponed. Run Collect again when ready.");
                    return;
                }
                QList<QJsonObject> selections;
                for (auto* item : fileList->selectedItems())
                {
                    selections.append(
                        QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object());
                }
                chosen = selections.takeFirst();
                for (const auto& extra : selections)
                {
                    ModLink extraLink = entry->link;
                    extraLink.file = jsonId(extra["file_id"]);
                    auto extraEntry =
                        enqueueMod(extraLink, entry->reason, entry->name, entry->notes);
                    if (!extraEntry)
                    {
                        finishModResolution(entry, "Could not add all selected files.");
                        return;
                    }
                    extraEntry->filename = extra["file_name"].toString();
                    extraEntry->version = extra["version"].toString();
                    extraEntry->size = extra["size_in_bytes"].toInteger();
                    extraEntry->resolved = false;
                    extraEntry->state = "Waiting to check";
                }
            }
            else
            {
                auto choices = available;
                if (choices.isEmpty())
                {
                    finishModResolution(entry, "No available files found.");
                    return;
                }
                QStringList labels;
                for (const auto& file : choices)
                {
                    labels << file["name"].toString() + " · " + file["version"].toString() + " · " +
                                  file["category_name"].toString() + " · ID " +
                                  jsonId(file["file_id"]);
                }
                bool ok;
                const auto selection = QInputDialog::getItem(this,
                    "Choose a file · " + entry->name,
                    "This mod needs a file choice. Check the game "
                    "version and author's notes.\n" +
                        entry->notes.left(500),
                    labels,
                    0,
                    false,
                    &ok);
                if (!ok)
                {
                    finishModResolution(
                        entry, "File selection postponed. Run Collect again when ready.");
                    return;
                }
                chosen = choices[labels.indexOf(selection)];
            }
        }
        entry->link.file = jsonId(chosen["file_id"]);
        entry->filename = chosen["file_name"].toString();
        entry->version = chosen["version"].toString();
        entry->size = chosen["size_in_bytes"].toInteger();
        if (entry->filename.isEmpty())
        {
            finishModResolution(entry, "Missing archive filename.");
            return;
        }
        if (legacy)
        {
            finishModResolution(entry);
        }
        else
        {
            resolveFileDependencies(entry);
        }
    });
}

void MainWindow::resolveFileDependencies(std::shared_ptr<QueuedMod> entry)
{
    requestJson("/v3/games/" + entry->link.game + "/mod-file-versions/" + entry->link.file,
        [this, entry](QJsonDocument document, QString error)
    {
        if (!error.isEmpty())
        {
            finishModResolution(entry, error);
            return;
        }
        const auto version = nmd::responseObject(document);
        const auto id = jsonId(version["id"]);
        if (id.isEmpty() || id == "0")
        {
            finishModResolution(entry, "Missing file version identity.");
            return;
        }
        requestJson("/v3/mod-file-versions/" + id + "/dependencies",
            [this, entry, id](QJsonDocument rawDoc, QString rawError)
        {
            if (!rawError.isEmpty())
            {
                finishModResolution(entry, rawError);
                return;
            }
            const auto raw = nmd::responseObject(rawDoc);
            if (!raw.contains("dependency_definitions") ||
                !raw.contains("dlc_dependency_definitions"))
            {
                finishModResolution(entry, "Incomplete file requirement metadata.");
                return;
            }
            for (const auto& value : raw["dlc_dependency_definitions"].toArray())
            {
                QStringList names;
                for (const auto& target : value.toObject()["dlc_targets"].toArray())
                {
                    names << target.toObject()["name"].toString();
                }
                auto note = "DLC required (one of): " + names.join(", ");
                entry->notes += "\n" + note;
                log(entry->name + " — " + note);
            }
            const auto expected = raw["dependency_definitions"].toArray();
            requestJson("/v3/mod-file-versions/" + id + "/dependencies/ranges/materialized",
                [this, entry, expected](QJsonDocument result, QString depError)
            {
                if (!depError.isEmpty())
                {
                    finishModResolution(entry, depError);
                    return;
                }
                const auto object = nmd::responseObject(result);
                if (!object.contains("dependencies"))
                {
                    finishModResolution(entry, "Missing resolved file requirements.");
                    return;
                }
                const auto definitions = object["dependencies"].toArray();
                QSet<QString> ids;
                for (const auto& definition : definitions)
                {
                    ids.insert(jsonId(definition.toObject()["id"]));
                }
                for (const auto& definition : expected)
                {
                    if (!ids.contains(jsonId(definition.toObject()["id"])))
                    {
                        finishModResolution(entry,
                            "A required file has no available matching "
                            "version. Review this mod on Nexus.");
                        return;
                    }
                }
                selectDependencyCandidates(entry, definitions);
            });
        });
    });
}

void MainWindow::selectDependencyCandidates(
    std::shared_ptr<QueuedMod> entry, const QJsonArray& definitions, int index)
{
    if (index >= definitions.size())
    {
        finishModResolution(entry);
        return;
    }
    const auto definition = definitions[index].toObject();
    QList<ModLink> candidates;
    QStringList labels;
    for (const auto& value : definition["candidate_mod_files"].toArray())
    {
        const auto file = value.toObject();
        const auto mod = file["mod"].toObject();
        for (const auto& v : file["candidate_versions"].toArray())
        {
            const auto version = v.toObject();
            const auto category = version["category"].toString();
            if (category == "removed" || category == "archived")
            {
                continue;
            }
            ModLink link{mod["game"].toObject()["domain_name"].toString(),
                jsonId(mod["game_scoped_id"]),
                jsonId(version["game_scoped_id"])};
            link = parseModLink(link.page().toString());
            if (!link.valid())
            {
                continue;
            }
            for (const auto& existing : m_queue)
            {
                if (existing->link.modKey() == link.modKey() && existing->link.file == link.file)
                {
                    selectDependencyCandidates(entry, definitions, index + 1);
                    return;
                }
            }
            candidates << link;
            labels << mod["name"].toString() + " · " + file["name"].toString() + " · " +
                          version["version"].toString() + " · ID " + link.file;
        }
    }
    if (candidates.isEmpty())
    {
        finishModResolution(
            entry, "A file requirement has no available candidate. Review it on Nexus.");
        return;
    }
    int choice = 0;
    if (candidates.size() > 1)
    {
        bool ok;
        const auto label = QInputDialog::getItem(this,
            "Required file · " + entry->name,
            "Choose one compatible file to satisfy this requirement.\nAn "
            "already queued matching version is reused automatically.",
            labels,
            0,
            false,
            &ok);
        if (!ok)
        {
            finishModResolution(
                entry, "Requirement selection postponed. Run Collect again when ready.");
            return;
        }
        choice = labels.indexOf(label);
    }
    if (!enqueueMod(candidates[choice], entry->name, labels[choice]))
    {
        finishModResolution(
            entry, "Could not add this requirement. Split the queue into smaller batches.");
        return;
    }
    selectDependencyCandidates(entry, definitions, index + 1);
}

void MainWindow::finishModResolution(std::shared_ptr<QueuedMod> entry, const QString& error)
{
    entry->resolved = true;
    if (!error.isEmpty())
    {
        entry->state = "Needs attention";
        entry->notes += "\n" + error;
        log(entry->name + " — " + error);
    }
    else if (!hasCompletedDownload(entry))
    {
        entry->state = "Ready to confirm";
    }
    refreshQueue();
    saveQueue();
    QTimer::singleShot(0, this, &MainWindow::resolveNextMod);
}
