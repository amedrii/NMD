# NMD — Nexus Mod Downloader

Lightweight Nexus Mod Downloader.

A small Windows desktop app for collecting Nexus mod links, resolving their requirements, and saving authorized downloads to one folder. Built with C++17 and Qt 6 Widgets.

## Start

Open **dist/NMD/NMD.exe**. Keep the adjacent DLLs and plugin folders with it.

1. Click **Get API key**, sign into Nexus, and generate your personal API key. Paste it into NMD. The key stays in memory and is never included in queue/history files. You can optionally supply it through the NEXUS_API_KEY environment variable.
2. Choose your download folder.
3. Paste one Nexus mod URL per line and click **Collect mods + requirements**.
4. If asked, select the file/version you want. Review author notes, off-site requirements, DLC, and any **Needs attention** rows.
5. Click **Connect Nexus download buttons** once. This registers NMD as your Windows account's nxm link handler; the app asks before replacing another manager. Click it again to restore the previous command.
6. Click **Open next download**. On Nexus, use **Mod manager download**, then **Slow download**. Allow your browser to open NMD. NMD downloads the authorized file to your chosen folder.
7. Repeat the Nexus confirmation for each queued file. The next file can be confirmed while an archive is downloading; transfers are processed one at a time.

Free accounts require Nexus's confirmation for each file. This app does not automate or bypass that step. If the browser cannot hand off the link, **Paste download link** accepts a signed nxm:// link obtained from Nexus. Plain mod-page URLs belong in the multiline input.

## Behavior

- Recursively follows Nexus requirements, with duplicate and cycle detection. A requirement already in the input/queue is reused.
- Uses the official v1 API for metadata, file listings and authorized downloads; v2 GraphQL for mod requirements; and v3 for file-specific dependency ranges.
- Reads **Nexus requirements**, not the reverse **Mods requiring this mod** list.
- Fetches all requirement pages. Incomplete metadata and unavailable dependencies are reported as needing attention.
- A single current main file is selected automatically. Multiple main files or file-level alternatives prompt for a choice. Compatible file versions already in the queue are reused.
- Different required files from the same mod can appear separately. A mod-level requirement is deduplicated by game and mod ID; a pinned file requirement also checks the file ID.
- Keeps author notes visible. Legacy requirement lists can include conditional or optional requirements. Review them on Nexus; NMD cannot infer compatibility from arbitrary prose.
- Shows off-site requirements and DLC as notes. These must be obtained or checked separately.
- Saves archives under `<folder>/<game>/<mod ID>/<file ID>-<archive name>`. It does not extract, install or execute mods.
- Keeps a local `.nmd-downloads.json` history in the download folder. Completed archives with matching paths and sizes are skipped. SHA-256 is recorded at completion, but existing archives are not rehashed on every queue check.
- Existing files without a matching completion record are preserved. Move the conflicting file or select a new folder.
- Streams downloads to temporary files and commits only successful, size-checked archives. Failed or stopped partial transfers are discarded; reconfirm on Nexus to retry.
- Queue and folder preferences persist under Qt's local application-data folder for **NMD / Nexus Mod Downloader**. API keys and signed download URLs are not persisted.
- The app handles a single running instance, forwarding browser download links into it.

## Build

Requirements: Windows, Qt 6.5+ with Widgets and Network, CMake 3.16+, and a matching C++17 compiler.

The supplied build script defaults to this machine's Qt 6.11.2 / MinGW 13.1 installation:

```powershell
.\build.ps1 -Package
```

Override installation locations with `-QtRoot` and `-ToolsRoot`. Or open CMakeLists.txt in Qt Creator and select the installed Qt desktop kit.

The packaged executable is written to `dist/NMD/NMD.exe`. Keep the whole directory together. Re-run the packaging script after rebuilding.

To create a ZIP for testers, run `.\package-release.ps1`. This also includes
runtime license notices and creates a SHA-256 checksum. See [RELEASING.md](RELEASING.md)
for publishing the source and ready-to-run downloads on GitHub.

## Validation

Automated tests run against simulated Nexus responses and temporary folders. They cover URL validation, expired/unsigned authorizations, recursive and shared dependencies, cycles, pagination, file-specific dependencies, unavailable requirements, account mismatches, archive size failures, retries, collision protection, history, secret-free persistence, and cancellation.

An offscreen screenshot of the real widget layout is produced at `build/app/app-preview.png` using simulated data.

**Live account integration has not been verified.** A real API key and user-confirmed Nexus download are needed to verify end-to-end behavior. Nexus's GraphQL and v3 dependency APIs can change. API failures are shown explicitly rather than assuming a mod has no requirements.

## Nexus references

- [API documentation](https://api-docs.nexusmods.com/)
- [GraphQL schema](https://graphql.nexusmods.com/)
- [Official client download-link behavior](https://github.com/Nexus-Mods/node-nexus-api/blob/master/docs/classes/_nexus_.nexus.md)
- [API acceptable use policy](https://help.nexusmods.com/article/114-api-acceptable-use-policy)

This is a personal-use/testing application. Nexus requires application registration before broader public distribution using its API. NMD identifies itself in API requests and sends account credentials only to api.nexusmods.com.

## Source layout

- main.cpp: application startup and forwarding browser links to the running instance.
- core.h / core.cpp: URL parsing, authorization validation and safe archive paths.
- queuedmod.h: the queue item model.
- mainwindow.cpp: queue state, duplicate handling, persistence and cancellation.
- mainwindow_ui.cpp: interface construction and Windows download-button registration.
- mainwindow_api.cpp: account validation, API requests and bounded retries.
- mainwindow_resolver.cpp: recursive mod and file dependency resolution.
- mainwindow_downloads.cpp: authorized link processing, streaming and atomic saves.
- workflowhelpers.h: small internal helpers shared by these modules.
- tests.cpp: offline API fixtures and workflow checks.

Formatting is defined in .clang-format. Use .\build.ps1 -Format -Package to format, build, test and package in one command.

