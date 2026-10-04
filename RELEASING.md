# GitHub source and downloadable releases

Use one public GitHub repository for the source and its Releases page for
ready-to-run Windows downloads. Do not commit `build/`, `dist/`, or credentials.

## First upload

Create an empty public repository on GitHub. Leave the initial README,
license, and gitignore options unchecked because this project already has
local files. Set the actual repository URL as the `origin` remote before
pushing. Commit the source, build scripts, documentation, and license notices.

Choose an explicit license for NMD if you want to grant others permission to
reuse or redistribute its source. Third-party license files do not choose the
license for this application's own code.

## Prepare the preview download

```powershell
.\package-release.ps1
```

The script builds and tests the app, deploys Qt, adds user-facing instructions
and library notices, and creates:

- `dist/NMD-v0.1.1p-windows-x64.zip`
- `dist/NMD-v0.1.1p-windows-x64.zip.sha256`

The ZIP contains the executable and its runtime libraries, not the source.

## Publish through GitHub Releases

Create a release tagged `v0.1.1p` from the corresponding source commit.
Use the text in `RELEASE-NOTES.md` and attach the ZIP and checksum. To make
GitHub show `0.1.1p` as the repository's **Latest** release, leave the
pre-release checkbox clear. GitHub also creates separate source-code archives.

Before a general public launch, verify live Nexus downloads, complete Nexus
application registration, and finalize the application's license and
third-party source-distribution arrangements. Keep access to the corresponding
third-party source available alongside binary releases as their licenses require.

Nexus policy: https://help.nexusmods.com/article/114-api-acceptable-use-policy
GitHub releases: https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases

## Later source and release updates

From the project folder, rebuild and package first:

```powershell
.\package-release.ps1
```

Then publish the source changes:

```powershell
git status
git add .
git commit -m "Describe the change"
git push origin main
```

For a new downloadable build, choose a new tag such as `v0.1.2p` on
GitHub's **Releases** page, paste the matching notes from `RELEASE-NOTES.md`,
attach the ZIP and its `.sha256` file from `dist/`, and leave **Set as a
pre-release** clear so GitHub marks it as the latest release. The tag should
point to the commit you just pushed.
