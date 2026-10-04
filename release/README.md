# NMD — Nexus Mod Downloader

Windows x64 preview build, version 0.1.1p.

## Run

Extract the entire ZIP to a permanent folder, then open `NMD.exe` inside the
`NMD` folder. Keep its DLLs and plugin folders together. Qt Creator, a compiler,
and the app's source code are not needed to run this download.

## Connect and download

1. Click **Get API key**, sign into Nexus, and generate your personal API key.
   Paste it into the app. Your key stays in memory for this session.
2. Choose a folder for the downloaded mod archives.
3. Paste one Nexus mod link per line and click **Collect mods + requirements**.
4. When a mod has several files, tick every archive you need in the file chooser
   (for example, the main archive and a texture archive). The main file starts
   selected, and each checked file becomes its own queue item.
5. Review requirement notes and any **Needs attention** rows.
6. Click **Connect Nexus download buttons** to register the app for Nexus
   download links. This can replace your current mod manager's link handler;
   click the button again to restore the previous handler.
7. Click **Open next download**, then choose **Mod manager download** and
   **Slow download** on Nexus. Let your browser open NMD.
8. Confirm each remaining file on Nexus. The app saves the authorized downloads
   and skips files already recorded as complete.

The app collects requirements recursively and avoids duplicates. It remembers
completed Nexus file IDs and versions, so the same version is not downloaded
twice while separate files such as a main archive and textures remain distinct.
It downloads
archives; it does not install, extract, or execute mods. Off-site requirements,
DLC, and conditional requirements in author notes need your review.

## Preview status

This build is for testing. Offline workflow tests pass, but live downloads have
not yet been verified with a real Nexus account. Nexus requires application
registration before general public distribution using its API.

An expired or failed download must be confirmed again on Nexus. Incomplete
transfers are discarded. Existing files without a matching NMD completion
record are preserved rather than overwritten.

## Included libraries

This package uses Qt 6.11.2 and the MinGW runtime. See `THIRD-PARTY-NOTICES.md`
and the `licenses` directory for their notices, license texts, and source links.
