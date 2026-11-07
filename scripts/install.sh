#!/bin/sh
MODE=""
FORK=""
TAG="${QC_VERSION:-}"
while [ $# -gt 0 ]; do
    case "$1" in
    --update) MODE=--update ;;
    --version)
        TAG=$2
        shift
        ;;
    --fork)
        FORK=$2
        shift
        ;;
    build) MODE=build ;;
    *) die "unknown option: $1" ;;
    esac
    shift
done
LLVM_VER=23
LLVM_FULL=${LLVM_FULL:-23.1.5}
MIN_GLIBCXX=3.4.32
MIN_MACOS=14.0
QC_HOME="${QC_HOME:-$HOME/.qc}"
REPO_URL="${FORK:-https://github.com/Youg-Otricked/QuarticC}"
LOG="${TMPDIR:-/tmp}/qc-install.log"
: >"$LOG"
PM_CMD=""
PM_UPDATED=""
GREEN=$(printf '\033[0;32m')
GLOW_GREEN=$(printf '\033[1;92m')
BLUE=$(printf '\033[0;34m')
RED=$(printf '\033[0;31m')
YELLOW=$(printf '\033[1;33m')
PURPLE=$(printf '\033[38;2;94;80;134m')
NC=$(printf '\033[0m')
info() { printf '%s==>%s %s\n' "$YELLOW" "$NC" "$*"; }
ok() { printf ' %s%s%s\n' "$GLOW_GREEN" "$*" "$NC"; }
warn() { printf ' %s! %s%s\n' "$YELLOW" "$*" "$NC" >&2; }
die() {
    printf ' %serror:%s %s\n' "$RED" "$NC" "$*" >&2
    exit 1
}
have_tty() { [ -z "${QC_YES:-}" ] && (: </dev/tty) 2>/dev/null; }
version_ge() {
    awk -v a="$1" -v b="$2" '
    BEGIN {
        na = split(a, A, ".")
        nb = split(b, B, ".")
        n = na > nb ? na : nb
        for (i = 1; i <= n; i++) {
            x = (i <= na ? A[i] + 0 : 0)
            y = (i <= nb ? B[i] + 0 : 0)
            if (x > y) exit 0
            if (x < y) exit 1
        }
        exit 0
    }'
}
ask() {
    have_tty || {
        ANSWER=$2
        return 0
    }
    hint='[y/N]'
    [ "$2" = y ] && hint='[Y/n]'
    printf '%s%s %s%s > ' "$PURPLE" "$1" "$hint" "$NC"
    read -r reply </dev/tty || reply=
    case "$reply" in
    y | Y) ANSWER=y ;;
    n | N) ANSWER=n ;;
    *) ANSWER=$2 ;;
    esac
}
add_line() {
    [ -f "$1" ] && grep -qF "$2" "$1" && return 0
    mkdir -p "$(dirname "$1")" 2>/dev/null || return 0
    printf '\n# QuarticC\n%s\n' "$2" >>"$1" || warn "could not edit $1"
}
SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1; then SUDO=sudo; fi
install_pkg() {
    pkg=$1
    if [ -z "$PM_CMD" ]; then
        if command -v apt-get >/dev/null 2>&1; then
            PM_CMD="apt-get install -y"
        elif command -v dnf >/dev/null 2>&1; then
            PM_CMD="dnf install -y"
        elif command -v yum >/dev/null 2>&1; then
            PM_CMD="yum install -y"
        elif command -v pacman >/dev/null 2>&1; then
            PM_CMD="pacman -S --needed --noconfirm"
        elif command -v zypper >/dev/null 2>&1; then
            PM_CMD="zypper --non-interactive install"
        elif command -v apk >/dev/null 2>&1; then
            PM_CMD="apk add"
        elif command -v xbps-install >/dev/null 2>&1; then
            PM_CMD="xbps-install -y"
        elif command -v brew >/dev/null 2>&1; then
            PM_CMD="brew install"
        else
            have_tty || die "no known package manager and no terminal to ask. Install '$pkg' manually."
            printf 'Could not detect a package manager.\n' >&2
            while :; do
                printf 'Enter your install command (e.g. "pacman -S --noconfirm"): ' >&2
                read -r PM_CMD </dev/tty
                bin=${PM_CMD%% *}
                [ -n "$bin" ] && command -v "$bin" >/dev/null 2>&1 && break
                printf '"%s" not found, try again.\n' "$bin" >&2
            done
        fi
    fi
    bin=${PM_CMD%% *}
    s=$SUDO
    [ "$bin" = brew ] && s=""
    printf '%s  installing %s (%s %s)%s\n' "$BLUE" "$pkg" "$PM_CMD" "$pkg" "$NC"
    if [ "$TEST" != y ]; then
        if [ -z "$PM_UPDATED" ]; then
            case "$bin" in
            apt-get) $s apt-get update -qq >>"$LOG" || true ;;
            pacman) $s pacman -Sy --noconfirm >>"$LOG" || true ;;
            esac
            PM_UPDATED=1
        fi
        $s $PM_CMD "$pkg" >>"$LOG" || {
            tail -n 15 "$LOG" >&2
            return 1
        }
    fi
    printf '%s  successfully installed %s%s\n' "$GREEN" "$pkg" "$NC"
}
need_pkg() { install_pkg "$1" || die "failed to install $1 (log: $LOG)"; }
find_llvm_config() {
    for c in "llvm-config-$LLVM_VER" "$QC_HOME"/llvm-"$LLVM_VER".*/bin/llvm-config "$(brew --prefix "llvm@$LLVM_VER" 2>/dev/null)/bin/llvm-config" llvm-config; do
        if command -v "$c" >/dev/null 2>&1 || [ -x "$c" ]; then
            v=$("$c" --version 2>/dev/null)
            case "$v" in
            "$LLVM_VER".*)
                echo "$c"
                return 0
                ;;
            esac
        fi
    done
    return 1
}
install_llvm_tarball() {
    case "$os-$cpu" in
    Linux-x86_64) asset="LLVM-$LLVM_FULL-Linux-X64.tar.xz" ;;
    Linux-arm64) asset="LLVM-$LLVM_FULL-Linux-ARM64.tar.xz" ;;
    Darwin-arm64) asset="LLVM-$LLVM_FULL-macOS-ARM64.tar.xz" ;;
    *) die "no prebuilt LLVM for $os/$cpu. Install LLVM $LLVM_VER manually and put llvm-config on your PATH." ;;
    esac
    command -v curl >/dev/null 2>&1 || need_pkg curl
    command -v xz >/dev/null 2>&1 || need_pkg xz
    dest="$QC_HOME/llvm-$LLVM_FULL"
    if [ ! -x "$dest/bin/llvm-config" ]; then
        info "Downloading LLVM $LLVM_FULL (large download)"
        mkdir -p "$dest"
        curl -fL --progress-bar "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_FULL/$asset" |
            tar -xJ -C "$dest" --strip-components=1 ||
            die "could not download or unpack $asset (check the asset name on the LLVM releases page)"
    fi
    LLVM_CONFIG="$dest/bin/llvm-config"
    LLVM_FROM_TARBALL=1
}
config() {
    STDLIB_BUILD=$2
    QC_BUILD=$1
    ok "Built C^4"
    info "Installing to $QC_HOME"
    mkdir -p "$QC_HOME/bin" "$QC_HOME/lib" "$QC_HOME/docs"
    if [ -f "$QC_HOME/lib/stdlib.qc" ] && ! cmp -s stdlib.qc "$QC_HOME/lib/stdlib.qc"; then
        cp "$QC_HOME/lib/stdlib.qc" "$QC_HOME/lib/stdlib.qc.bak"
        warn "existing stdlib saved as stdlib.qc.bak"
    fi
    cp $QC_BUILD "$QC_HOME/bin/qc"
    cp $STDLIB_BUILD "$QC_HOME/lib/stdlib.qc"
    cp -R docs/. "$QC_HOME/docs/"
    cat >"$QC_HOME/env" <<'EOF'
#!/bin/sh
# QuarticC env
case ":${PATH}:" in *":$HOME/.qc/bin:"*) ;; *) export PATH="$HOME/.qc/bin:$PATH" ;; esac
export QC_INSTALL="$HOME/.qc/bin/qc"
export QC_STDLIB="$HOME/.qc/lib/stdlib.qc"
EOF
    cat >"$QC_HOME/env.fish" <<'EOF'
# QuarticC env
fish_add_path -g $HOME/.qc/bin
set -gx QC_INSTALL $HOME/.qc/bin/qc
set -gx QC_STDLIB $HOME/.qc/lib/stdlib.qc
EOF
    src='. "$HOME/.qc/env"'
    cur=$(basename "${SHELL:-sh}")
    zrc="${ZDOTDIR:-$HOME}/.zshrc"
    add_line "$HOME/.profile" "$src"
    if [ "$cur" = bash ] || [ -f "$HOME/.bashrc" ]; then
        add_line "$HOME/.bashrc" "$src"
    fi
    if [ "$os" = Darwin ] && { [ "$cur" = bash ] || [ -f "$HOME/.bash_profile" ]; }; then
        add_line "$HOME/.bash_profile" "$src"
    fi
    if [ "$cur" = zsh ] || [ -f "$zrc" ]; then
        add_line "$zrc" "$src"
    fi
    if [ "$cur" = fish ] || command -v fish >/dev/null 2>&1; then
        add_line "$HOME/.config/fish/config.fish" 'source $HOME/.qc/env.fish'
    fi
    case "$cur" in
    bash | zsh | fish | sh | dash | ash) ;;
    *) warn "unrecognised shell \"$cur\": add $QC_HOME/bin to your PATH manually" ;;
    esac
    ok "Finished setup"
    printf '\n%sqc installed!%s Restart your shell, or run:\n  . "%s/env"\n\n%sThank you for using C^4!%s\n' \
        "$GLOW_GREEN" "$NC" "$QC_HOME" "$GLOW_GREEN" "$NC"
}
clone_repo() {
    if [ -n "$TAG" ]; then
        git clone --depth 1 --branch "$TAG" "$REPO_URL" "$SRC/QuarticC" >>"$LOG" 2>&1 ||
            die "failed to clone $REPO_URL at '$TAG' (does the tag exist?)"
    else
        git clone --depth 1 "$REPO_URL" "$SRC/QuarticC" >>"$LOG" 2>&1 ||
            die "failed to clone $REPO_URL"
    fi
}
compile_source() {
    need_pkg git
    need_pkg cmake
    command -v make >/dev/null 2>&1 || need_pkg make
    if ! command -v g++ >/dev/null 2>&1 && ! command -v c++ >/dev/null 2>&1; then
        case "${PM_CMD%% *}" in
        apt-get | apk) need_pkg g++ ;;
        brew) ;;
        *) case "$family" in *fedora* | *rhel* | *suse*) need_pkg gcc-c++ ;; *) need_pkg gcc ;; esac ;;
        esac
    fi
    llvm_bin=$("$LLVM_CONFIG" --bindir)
    llvm_lib=$("$LLVM_CONFIG" --libdir)
    case "$os" in
    Linux)
        llvm_shared="$llvm_lib/libLLVM.so"
        if [ ! -e "$llvm_shared" ]; then
            echo "LLVM library directory: $llvm_lib"
            echo "Contents:"
            ls -lah "$llvm_lib"

            echo "Searching for LLVM shared libraries..."
            find "$llvm_lib" -name '*LLVM*' -print
            llvm_shared=$(find "$llvm_lib" \( -name 'libLLVM.so.*' -o -name 'libLLVM-*.so' \) \( -type f -o -type l \) |
                head -n 1)
            [ -n "$llvm_shared" ] ||
                die "LLVM shared library not found in $llvm_lib"
            ln -s "$(basename "$llvm_shared")" "$llvm_lib/libLLVM.so" ||
                die "could not create libLLVM.so symlink"
        fi
        ;;
    Darwin)
        llvm_shared="$llvm_lib/libLLVM.dylib"
        if [ ! -e "$llvm_shared" ]; then
            llvm_shared=$(find "$llvm_lib" -name 'libLLVM*.dylib' \( -type f -o -type l \) |
                head -n 1)
            [ -n "$llvm_shared" ] ||
                die "LLVM dylib not found in $llvm_lib"
            ln -s "$(basename "$llvm_shared")" "$llvm_lib/libLLVM.dylib" ||
                die "could not create libLLVM.dylib symlink"
        fi
        ;;
    *)
        die "unsupported operating system: $os"
        ;;
    esac
    llvm_prefix=$("$LLVM_CONFIG" --prefix)
    clang_cfg_find() {
        find "$llvm_prefix" -maxdepth 4 -name ClangConfig.cmake 2>/dev/null | head -n1
    }
    clang_ok() {
        [ -n "$LLVM_FROM_TARBALL" ] && return 0
        [ -n "$(clang_cfg_find)" ] || return 1
        case "$family" in
        *debian* | *ubuntu*) [ -f "$llvm_lib/libclangBasic.a" ] ;;
        *) return 0 ;;
        esac
    }
    if ! clang_ok; then
        case "$os-$family" in
        Linux-*debian* | Linux-*ubuntu*) need_pkg "libclang-$LLVM_VER-dev" ;;
        Linux-*fedora* | Linux-*rhel*)
            install_pkg clang-devel || die "failed to install clang-devel (log: $LOG)"
            ;;
        *) need_pkg clang ;;
        esac
    fi
    clang_ok || die "Clang development files for LLVM $LLVM_VER not found under $llvm_prefix"
    clang_cmake=$(dirname "$(clang_cfg_find)")
    if [ -x "$llvm_bin/clang" ]; then
        CC="$llvm_bin/clang"
        CXX="$llvm_bin/clang++"
    elif command -v "clang-$LLVM_VER" >/dev/null 2>&1; then
        CC="clang-$LLVM_VER"
        CXX="clang++-$LLVM_VER"
    else
        need_pkg clang
        CC=clang
        CXX=clang++
        warn "using the system clang, which may not be version $LLVM_VER"
    fi
    if [ ! -f "$llvm_lib/libclangBasic.a" ] && [ -z "${LLVM_FROM_TARBALL:-}" ]; then
        case "$os-$family" in
        Linux-*debian* | Linux-*ubuntu*) need_pkg "libclang-$LLVM_VER-dev" ;;
        Linux-*fedora* | Linux-*rhel*) need_pkg "clang-devel" ;;
        *) need_pkg clang ;;
        esac
    fi
    case "$family" in
    *debian* | *ubuntu*)
        [ -f "$llvm_lib/libclangBasic.a" ] ||
            die "Clang development libraries for LLVM $LLVM_VER are missing (no libclangBasic.a in $llvm_lib)"
        ;;
    esac
    case "$os-$family" in
    Linux-*debian* | Linux-*ubuntu*)
        for p in zlib1g-dev libzstd-dev libedit-dev libffi-dev libxml2-dev libcurl4-openssl-dev; do
            need_pkg "$p"
        done
        ;;
    Linux-*fedora* | Linux-*rhel*)
        for p in zlib-devel libzstd-devel libedit-devel libffi-devel libxml2-devel libcurl-devel; do
            need_pkg "$p"
        done
        ;;
    Linux-*arch*)
        for p in zlib zstd libedit libffi libxml2 curl; do
            need_pkg "$p"
        done
        ;;
    esac
    ok "Installed dependencies"
    info "Cloning repo"
    printf '%sContinue? [Y/n]%s > ' "$PURPLE" "$NC"
    if have_tty; then
        read -r choice </dev/tty
    else
        choice=y
    fi
    case "$choice" in
    n | N) exit 1 ;;
    esac
    [ "$TEST" = y ] && return 0
    SRC=$(mktemp -d "${TMPDIR:-/tmp}/qc-src.XXXXXX") || die "could not create temp dir"
    trap 'rm -rf "$SRC"' 0 2 15
    clone_repo
    cd "$SRC/QuarticC" || die "failed to enter repo"
    ok "Cloned repo"
    info "Setting up CMake"
    targets=$("$LLVM_CONFIG" --targets-built)
    if [ "$targets" = "X86" ]; then
        info "LLVM only has the X86 target, building x86-only"
        x86=y
    elif [ -n "${QC_X86_ONLY:-}" ]; then
        x86=y
    else
        ask "Only compile for the x86-64 backend?" n
        x86=$ANSWER
    fi
    ask "Compile at -O0 with debug symbols?" n
    dbg=$ANSWER
    build_type=Release
    [ "$dbg" = y ] && build_type=Debug
    set -- -S . -B build -DCMAKE_BUILD_TYPE="$build_type" -DLLVM_DIR="$("$LLVM_CONFIG" --cmakedir)" -DClang_DIR="$clang_cmake" -DCMAKE_PREFIX_PATH="$("$LLVM_CONFIG" --prefix)" -DCMAKE_BUILD_RPATH="$llvm_lib" -DCMAKE_INSTALL_RPATH="$llvm_lib"
    [ "$x86" = y ] && set -- "$@" -DX86_ONLY=On
    log="${TMPDIR:-/tmp}/qc-build.log"
    CC=$CC CXX=$CXX cmake "$@" >"$log" 2>&1 || {
        tail -n 30 "$log" >&2
        die "CMake failed (full log: $log)"
    }
    ok "Configured"
    cores=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null ||
        sysctl -n hw.ncpu 2>/dev/null || echo 1)
    case "$cores" in '' | *[!0-9]*) cores=1 ;; esac
    [ "$cores" -gt 0 ] || cores=1
    info "Building with $cores cores"
    cmake --build build -j"$cores" >>"$log" 2>&1 || {
        tail -n 30 "$log" >&2
        die "build failed (full log: $log)"
    }
    ok "Built C^4"
    config build/qc stdlib.qc
}
os=$(uname -s)
cpu=$(uname -m)
case "$cpu" in
arm64 | aarch64) cpu=arm64 ;;
x86_64 | amd64) cpu=x86_64 ;;
esac
if [ "$os" = Darwin ] && [ "$cpu" = x86_64 ] &&
    [ "$(sysctl -n hw.optional.arm64 2>/dev/null)" = 1 ]; then
    cpu=arm64
fi
family=""
[ "$os" = Linux ] && family=$(
    . /etc/os-release 2>/dev/null
    echo "${ID_LIKE:-$ID}"
)
printf '%s========================\n   QuarticC Installer\n========================%s\n\n\n' "$GREEN" "$NC"
if [ "$MODE" = "build" ] || [ "$MODE" = "" ]; then
    printf '%sInstall method:%s\n' "$PURPLE" "$NC"
    printf '  [c/y] compile from source (recommended)\n'
    printf '  [l] latest precompiled (ARM64 macOS and recent Linux only)\n'
    printf '> '
    if have_tty; then
        read -r choice </dev/tty
    else
        choice=c
    fi
    printf '\n'
    case "$choice" in
    l | L) mode=prebuilt ;;
    *) mode=source ;;
    esac
    printf '%sInstalling Dependencies%s\n' "$YELLOW" "$NC"
    LLVM_FROM_TARBALL=""
    if LLVM_CONFIG=$(find_llvm_config); then
        printf '%s  LLVM %s found (%s)%s\n' "$GREEN" "$LLVM_VER" "$LLVM_CONFIG" "$NC"
    else
        case "$os" in
        Darwin) need_pkg "llvm@$LLVM_VER" ;;
        Linux)
            case "$family" in
            *debian* | *ubuntu*)
                command -v curl >/dev/null 2>&1 || need_pkg curl
                need_pkg wget
                need_pkg gnupg
                need_pkg lsb-release
                need_pkg software-properties-common
                tmp=$(mktemp)
                curl -fsSL https://apt.llvm.org/llvm.sh -o "$tmp" || die "could not download llvm.sh"
                ${SUDO:+$SUDO} bash "$tmp" "$LLVM_VER" >>"$LOG" 2>&1 </dev/null || {
                    tail -n 15 "$LOG" >&2
                    die "llvm.sh failed (log: $LOG)"
                }
                need_pkg "llvm-$LLVM_VER-dev"
                ;;
            *fedora* | *rhel*) install_pkg "llvm$LLVM_VER-devel" || install_pkg llvm-devel ;;
            *arch*) install_pkg "llvm" ;;
            *) install_llvm_tarball ;;
            esac
            ;;
        *) install_llvm_tarball ;;
        esac
        [ -n "$LLVM_FROM_TARBALL" ] || LLVM_CONFIG=$(find_llvm_config) || {
            warn "package manager didn't provide LLVM $LLVM_VER, using the official tarball"
            install_llvm_tarball
        }
    fi
    can_prebuilt=0
    if [ "$mode" = prebuilt ]; then
        case "$os-$cpu" in
        Darwin-arm64)
            if [ "$cpu" = arm64 ] && m=$(sw_vers -productVersion 2>/dev/null) &&
                version_ge "$m" "$MIN_MACOS"; then
                can_prebuilt=1
            fi
            ;;
        Linux-x86_64)
            lib=$(ldconfig -p 2>/dev/null | awk '/libstdc\+\+\.so\.6 /{print $NF; exit}')
            if [ -z "$lib" ]; then
                for d in /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu; do
                    [ -e "$d/libstdc++.so.6" ] && lib="$d/libstdc++.so.6" && break
                done
            fi
            if [ -n "$lib" ] && { [ "$cpu" = x86_64 ] || [ "$cpu" = arm64 ]; }; then
                x=$(grep -ao 'GLIBCXX_[0-9.]*' "$lib" | sort -Vu | tail -1 | sed 's/GLIBCXX_//')
                version_ge "$x" "$MIN_GLIBCXX" && can_prebuilt=1
            fi
            ;;
        esac
    fi
    if [ "$mode" = prebuilt ] && [ "$can_prebuilt" = 1 ]; then
        info 'Installing prebuilt C^4'
        case "$os-$cpu" in
        Linux-x86_64) artifact="qc-linux" ;;
        Darwin-arm64) artifact="qc-macos" ;;
        *) die "${RED}  No prebuilt C^4 exists for this platform. Compile from source.${NC}\n" ;;
        esac
        need_pkg curl
        TMP_INSTALL=$(mktemp -d)
        trap 'rm -rf $TMP_INSTALL' 0 2 15
        curl -fL -o "$TMP_INSTALL/qc" "$REPO_URL/releases/latest/download/$artifact" || die "could not download prebuilt"
        curl -fL -o "$TMP_INSTALL/stdlib" "$REPO_URL/releases/latest/download/stdlib.qc" || die "could not download prebuilt"
        ok "Installed prebuilt"
        config "$TMP_INSTALL/qc" "$TMP_INSTALL/stdlib"
    else
        if [ "$mode" = prebuilt ]; then
            printf '%s  This platform lacks access to prebuilt C^4. Compiling from source.%s\n' "$RED" "$NC"
        fi
        compile_source
    fi
    echo "Installation complete! Installation locations:"
    echo " ~/.qc/bin/qc"
    echo " ~/.qc/lib/stdlib.qc"
    echo " ~/.qc/docs"
    if [ -z "${QC_YES:-}" ]; then
        ask "Create demo program?" y
        if [ "$ANSWER" = 'y' ]; then
            printf '
int main() {
    `qout("Hello, World!\\n");
    return 0;
}
' | tee demo.qc
            info "sourcing env"
            . "$QC_HOME/env"
            info "compiling: qc demo.qc"
            qc demo.qc
            ./a.out
            rm a.out
            info "removing test program"
            rm demo.qc
        fi
    fi
else
    if [ "$MODE" = "--update" ]; then
        echo 'this assumes you already have all neccesary packages installed'
        ask 'only update the stdlib?' n
        stdOnly=$ANSWER
        if [ "$stdOnly" = 'y' ]; then
            info "Copying to $QC_HOME"
            if [ -f "$QC_HOME/lib/stdlib.qc" ] && ! cmp -s stdlib.qc "$QC_HOME/lib/stdlib.qc"; then
                cp "$QC_HOME/lib/stdlib.qc" "$QC_HOME/lib/stdlib.qc.bak"
                ok "existing stdlib saved as stdlib.qc.bak"
            fi
            curl -fL -o "$QC_HOME/lib/stdlib.qc" "https://raw.githubusercontent.com/Youg-Otricked/QuarticC/${TAG:-master}/stdlib.qc"
            info "Updated"
        else
            info "Reinstalling & Compiling C^4"
            if LLVM_CONFIG=$(find_llvm_config); then
                printf '%s  LLVM %s found (%s)%s\n' "$GREEN" "$LLVM_VER" "$LLVM_CONFIG" "$NC"
            fi
            llvm_bin=$("$LLVM_CONFIG" --bindir)
            llvm_lib=$("$LLVM_CONFIG" --libdir)
            llvm_prefix=$("$LLVM_CONFIG" --prefix)
            clang_cfg_find() {
                find "$llvm_prefix" -maxdepth 4 -name ClangConfig.cmake 2>/dev/null | head -n1
            }
            clang_ok() {
                [ -n "$LLVM_FROM_TARBALL" ] && return 0
                [ -n "$(clang_cfg_find)" ] || return 1
                case "$family" in
                *debian* | *ubuntu*) [ -f "$llvm_lib/libclangBasic.a" ] ;;
                *) return 0 ;;
                esac
            }
            if ! clang_ok; then
                case "$os-$family" in
                Linux-*debian* | Linux-*ubuntu*) need_pkg "libclang-$LLVM_VER-dev" ;;
                Linux-*fedora* | Linux-*rhel*)
                    install_pkg clang-devel || die "failed to install clang-devel (log: $LOG)"
                    ;;
                *) need_pkg clang ;;
                esac
            fi
            clang_ok || die "Clang development files for LLVM $LLVM_VER not found under $llvm_prefix"
            clang_cmake=$(dirname "$(clang_cfg_find)")
            if [ -x "$llvm_bin/clang" ]; then
                CC="$llvm_bin/clang"
                CXX="$llvm_bin/clang++"
            elif command -v "clang-$LLVM_VER" >/dev/null 2>&1; then
                CC="clang-$LLVM_VER"
                CXX="clang++-$LLVM_VER"
            else
                need_pkg clang
                CC=clang
                CXX=clang++
                warn "using the system clang, which may not be version $LLVM_VER"
            fi
            ok "Reverified Clang"
            info "Cloning repo"
            printf '%sContinue? [Y/n]%s > ' "$PURPLE" "$NC"
            if have_tty; then
                read -r choice </dev/tty
            else
                choice=y
            fi
            case "$choice" in
            n | N) exit 1 ;;
            esac
            SRC=$(mktemp -d "${TMPDIR:-/tmp}/qc-src.XXXXXX") || die "could not create temp dir"
            trap 'rm -rf "$SRC"' 0 2 15
            clone_repo
            cd "$SRC/QuarticC" || die "failed to enter repo"
            ok "Cloned repo"
            info "Setting up CMake"
            targets=$("$LLVM_CONFIG" --targets-built)
            if [ "$targets" = "X86" ]; then
                info "LLVM only has the X86 target, building x86-only"
                x86=y
            elif [ -n "${QC_X86_ONLY:-}" ]; then
                x86=y
            else
                ask "Only compile for the x86-64 backend?" n
                x86=$ANSWER
            fi
            ask "Compile at -O0 with debug symbols?" n
            dbg=$ANSWER
            build_type=Release
            [ "$dbg" = y ] && build_type=Debug
            set -- -S . -B build -DCMAKE_BUILD_TYPE="$build_type" -DLLVM_DIR="$("$LLVM_CONFIG" --cmakedir)" -DClang_DIR="$clang_cmake" -DCMAKE_PREFIX_PATH="$("$LLVM_CONFIG" --prefix)" -DCMAKE_BUILD_RPATH="$llvm_lib" -DCMAKE_INSTALL_RPATH="$llvm_lib"
            [ "$x86" = y ] && set -- "$@" -DX86_ONLY=On
            log="${TMPDIR:-/tmp}/qc-build.log"
            CC=$CC CXX=$CXX cmake "$@" >"$log" 2>&1 || {
                tail -n 30 "$log" >&2
                die "CMake failed (full log: $log)"
            }
            ok "Configured"
            cores=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null ||
                sysctl -n hw.ncpu 2>/dev/null || echo 1)
            case "$cores" in '' | *[!0-9]*) cores=1 ;; esac
            [ "$cores" -gt 0 ] || cores=1
            info "Building with $cores cores"
            cmake --build build -j"$cores" >>"$log" 2>&1 || {
                tail -n 30 "$log" >&2
                die "build failed (full log: $log)"
            }
            ok "Built C^4"
            cp build/qc "$QC_HOME/bin/qc"
            cp stdlib.qc "$QC_HOME/lib/stdlib.qc"
        fi
    else
        die "invalid option"
    fi
fi
