# NMD 0.1.0-preview.1

First Windows x64 testing build of Nexus Mod Downloader.

Download **NMD-v0.1.0-preview.1-windows-x64.zip**, extract the whole archive, and
run **NMD/NMD.exe**. No compiler or source checkout is required. The automatic
GitHub **Source code** downloads are intended for developers.

## Included

- Multiline Nexus mod links and a selectable download folder.
- Recursive requirement collection with duplicate and cycle detection.
- File-specific dependency handling and file/version choices.
- Authorized free-account downloads after confirmation on Nexus.
- Download progress, cancellation, safe file writes, and completion history.

## Testing limitations

- Automated tests use simulated Nexus responses. Live account integration
  remains unverified.
- Nexus application registration is needed before a general public launch.
- Off-site dependencies, DLC, and conditional author notes require review.
- Archives are downloaded, not installed. Failed transfers restart after a
  new confirmation on Nexus.

The accompanying `.sha256` file records the archive's SHA-256 checksum.
