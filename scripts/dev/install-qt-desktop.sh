#!/usr/bin/env bash
# Install a desktop Qt 6.12 development environment for Member C work.
#
# The target machine has no Qt, cmake, uic, moc, rcc or lrelease, and no sudo
# password is available. Everything therefore lands in .devtools/ inside the
# repository (git-ignored) and is activated by sourcing .devtools/env.sh.
#
# Usage:
#   scripts/dev/install-qt-desktop.sh            # install (idempotent)
#   source .devtools/env.sh                      # activate in a shell
#
# Notes:
#   * GL/GLX headers are genuinely missing from the image and are needed by
#     lc_glextensions.h, so they are unpacked into the local sysroot from the
#     distribution .deb files instead of being installed system-wide.
#   * Several xcb libraries required by Qt's xcb platform plugin are also
#     missing at runtime; they are unpacked into .devtools/sysroot/lib the same
#     way. No system file is modified and nothing needs root.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEVTOOLS="$REPO_ROOT/.devtools"
QT_VERSION="${QT_VERSION:-6.12.0}"
QT_ARCH="${QT_ARCH:-linux_gcc_64}"
QT_DIR="$DEVTOOLS/Qt"
SYSROOT="$DEVTOOLS/sysroot"
DEB_CACHE="$DEVTOOLS/debs"

log() { printf '[install-qt-desktop] %s\n' "$*"; }

mkdir -p "$DEVTOOLS" "$QT_DIR" "$SYSROOT" "$DEB_CACHE"

# ---------------------------------------------------------------- aqtinstall
if ! python3 -c 'import aqt' >/dev/null 2>&1; then
    log "installing aqtinstall into $DEVTOOLS/pylib"
    python3 -m pip install --quiet --disable-pip-version-check \
        --target "$DEVTOOLS/pylib" aqtinstall
fi

AQT_PYTHON() {
    PYTHONPATH="$DEVTOOLS/pylib${PYTHONPATH:+:$PYTHONPATH}" python3 -m aqt "$@"
}

# ---------------------------------------------------------------------- Qt
if [ ! -x "$QT_DIR/$QT_VERSION/gcc_64/bin/qmake" ]; then
    log "downloading Qt $QT_VERSION $QT_ARCH (~1.5 GB, one time)"
    AQT_PYTHON install-qt linux desktop "$QT_VERSION" "$QT_ARCH" \
        --outputdir "$QT_DIR"
else
    log "Qt $QT_VERSION already present"
fi

QMAKE="$QT_DIR/$QT_VERSION/gcc_64/bin/qmake"
QTBIN="$(dirname "$QMAKE")"
QTLIBEXEC="$(dirname "$QTBIN")/libexec"

# Qt 6 keeps the code generators (moc, uic, rcc) in libexec/, not bin/. qmake
# locates them by itself, but they are also put on PATH for ad-hoc use, so both
# directories are searched and both exported.
tool_path() {
    local tool="$1"
    if [ -x "$QTBIN/$tool" ]; then
        printf '%s\n' "$QTBIN/$tool"
    elif [ -x "$QTLIBEXEC/$tool" ]; then
        printf '%s\n' "$QTLIBEXEC/$tool"
    else
        return 1
    fi
}

for tool in qmake moc uic rcc lrelease lupdate; do
    tool_path "$tool" >/dev/null || {
        log "ERROR: missing Qt tool $tool in $QTBIN or $QTLIBEXEC"; exit 1; }
done
MOC="$(tool_path moc)"
UIC="$(tool_path uic)"
RCC="$(tool_path rcc)"
log "moc=$MOC"
log "uic=$UIC"
log "rcc=$RCC"

# ---------------------------------------------------------------- sysroot
# Packages needed for the *build* (headers / link stubs).
BUILD_DEBS=(
    libgl-dev
    libglx-dev
    mesa-common-dev
    libglvnd-dev
    libgl1-mesa-dev
)
# Packages needed at *runtime* by the xcb platform plugin and font stack.
RUNTIME_DEBS=(
    libxcb-cursor0
    libxcb-icccm4
    libxcb-keysyms1
    libxcb-image0
    libxcb-xkb1
    libxkbcommon-x11-0
    # Transitive dependencies of the Qt xcb platform plugin. Discovered by
    # running `ldd` on plugins/platforms/libqxcb.so after unpacking the packages
    # above; without them the plugin refuses to load at runtime.
    libxcb-util1
    libxcb-render-util0
)

unpack_deb() {
    local pkg="$1"
    local deb
    deb="$(ls "$DEB_CACHE/${pkg}"_*.deb 2>/dev/null | head -n1 || true)"
    if [ -z "$deb" ]; then
        ( cd "$DEB_CACHE" && apt-get download "$pkg" >/dev/null 2>&1 ) || {
            log "ERROR: cannot download $pkg"; return 1; }
        deb="$(ls "$DEB_CACHE/${pkg}"_*.deb | head -n1)"
    fi
    if [ ! -d "$SYSROOT/.stamp-${pkg}" ]; then
        dpkg-deb -x "$deb" "$SYSROOT"
        mkdir -p "$SYSROOT/.stamp-${pkg}"
        log "unpacked $pkg"
    fi
}

for pkg in "${BUILD_DEBS[@]}" "${RUNTIME_DEBS[@]}"; do
    unpack_deb "$pkg" || true
done

# libgl-dev ships libGL.so as a symlink to libGL.so.1, but the runtime package
# that provides the target is not unpacked here, which leaves a broken link and
# a linker that cannot resolve -lGL. Point it at the real system library.
GL_LIB_DIR="$SYSROOT/usr/lib/x86_64-linux-gnu"
if [ -L "$GL_LIB_DIR/libGL.so" ] && [ ! -e "$GL_LIB_DIR/libGL.so" ]; then
    for system_gl in /lib/x86_64-linux-gnu/libGL.so.1 /usr/lib/x86_64-linux-gnu/libGL.so.1; do
        if [ -e "$system_gl" ]; then
            ln -sf "$system_gl" "$GL_LIB_DIR/libGL.so.1"
            ln -sf libGL.so.1 "$GL_LIB_DIR/libGL.so"
            log "relinked libGL.so -> $system_gl"
            break
        fi
    done
fi

# GL headers land in usr/include/GL; Qt/platform code expects the GL include
# dir on the include path.
GL_INCLUDE=""
if [ -d "$SYSROOT/usr/include/GL" ]; then
    GL_INCLUDE="$SYSROOT/usr/include"
fi

# ------------------------------------------------------------------- env.sh
ENV_FILE="$DEVTOOLS/env.sh"
cat > "$ENV_FILE" <<EOF
# Generated by scripts/dev/install-qt-desktop.sh -- source this file.
export QTDIR="$QT_DIR/$QT_VERSION/gcc_64"
export PATH="\$QTDIR/bin:$QTLIBEXEC:$DEVTOOLS/pylib-bin:\$PATH"
export QT_HOST_PATH="\$QTDIR"
export LD_LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu:$SYSROOT/lib/x86_64-linux-gnu\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu\${LIBRARY_PATH:+:\$LIBRARY_PATH}"
export CPATH="${GL_INCLUDE}\${CPATH:+:\$CPATH}"
export CMAKE_PREFIX_PATH="\$QTDIR\${CMAKE_PREFIX_PATH:+:\$CMAKE_PREFIX_PATH}"
EOF

log "wrote $ENV_FILE"
"$QTBIN/qmake" -query QT_VERSION >/dev/null
log "Qt version: $("$QTBIN/qmake" -query QT_VERSION)"
log "done. Run:  source .devtools/env.sh"
