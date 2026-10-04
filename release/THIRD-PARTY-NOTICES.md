# Third-party software

NMD dynamically links to the following unmodified libraries from the installed
Qt 6.11.2 / MinGW 13.1 distribution:

- Qt Core, Gui, Widgets, Network, Svg, and the deployed Qt plugins.
  Copyright The Qt Company Ltd. and other contributors.
  Qt modules used here are available under LGPL version 3; the accompanying
  GPL version 3 text forms part of that license. Additional components carry
  their own licenses, reproduced in the Qt attribution documents.
- GCC runtime libraries: `libgcc_s_seh-1.dll` and `libstdc++-6.dll`.
  See the GCC license texts and GCC Runtime Library Exception.
- MinGW-w64 and winpthreads: see their accompanying license notices.

License texts and Qt attribution documents are included under `licenses`.
Attribution documents are copied from the installed Qt documentation and may
also describe optional components not present in this build.

The Qt DLLs are separate, replaceable files. To use a compatible modified Qt
build, close NMD, back up its DLLs and plugin folders, and replace them with
the corresponding files from your Windows x64 MinGW Qt build. Keep the Qt
modules and plugins compatible with one another. NMD does not check vendor
signatures or prevent replacement of these libraries. Rights granted by the
third-party licenses, including modification and debugging of those libraries,
are not restricted by this application.

## Upstream source and build information

- Qt 6.11.2 source archive index:
  https://download.qt.io/archive/qt/6.11/6.11.2/submodules/
- Qt Base (Core, Gui, Widgets, Network and associated plugins):
  https://download.qt.io/archive/qt/6.11/6.11.2/submodules/qtbase-everywhere-src-6.11.2.tar.xz
- Qt SVG:
  https://download.qt.io/archive/qt/6.11/6.11.2/submodules/qtsvg-everywhere-src-6.11.2.tar.xz
- Qt build instructions: https://doc.qt.io/qt-6/build-sources.html
- GCC 13.1 source: https://gcc.gnu.org/releases.html
- MinGW-w64 source: https://www.mingw-w64.org/downloads/

The application's build instructions are in the source repository's README.
These notices describe third-party components; they do not select a license
for NMD's own source code.
