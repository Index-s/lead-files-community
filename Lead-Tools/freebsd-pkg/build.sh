#!/bin/sh
#
# build.sh - build the Lead server and package it as a FreeBSD/amd64 .pkg.
#
# Run on a FreeBSD 14.x amd64 host (your QEMU VM or a CI FreeBSD VM). Produces
#   <outdir>/lead-server-<version>.pkg
#
# Usage:
#   Lead-Tools/freebsd-pkg/build.sh [options]
#     --version <v>     package version (default: git describe, else date)
#     --outdir  <dir>   where to write the .pkg (default: Lead-Tools/freebsd-pkg/dist)
#     --install-deps    pkg install the build dependencies first (needs root)
#     --no-build        skip compiling; reuse existing Lead-Server-Source/{game,db}
#     --mariadb <pkg>   MariaDB server package to depend on
#                       (default: mariadb1011-server)
#
# This script only WRITES a package; publishing to a repo is make-repo.sh.
#
set -eu

# --- locate ourselves / repo root -------------------------------------------
SELF="$(realpath "$0")"
PKGDIR="$(dirname "$SELF")"                 # Lead-Tools/freebsd-pkg
ROOT="$(realpath "${PKGDIR}/../..")"        # repo root

SRC="${ROOT}/Lead-Server-Source"
SVF="${ROOT}/Lead-Serverfiles"
DBS="${ROOT}/Lead-Database-Scripts"

# --- defaults / args --------------------------------------------------------
VERSION=""
OUTDIR="${PKGDIR}/dist"
INSTALL_DEPS=0
DO_BUILD=1
MARIADB_PKG=""   # empty = auto-detect the latest mariadb*-server available

while [ $# -gt 0 ]; do
	case "$1" in
		--version) VERSION="$2"; shift 2 ;;
		--outdir)  OUTDIR="$2"; shift 2 ;;
		--install-deps) INSTALL_DEPS=1; shift ;;
		--no-build) DO_BUILD=0; shift ;;
		--mariadb) MARIADB_PKG="$2"; shift 2 ;;
		-h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) echo "build.sh: unknown argument: $1" >&2; exit 2 ;;
	esac
done

if [ -z "$VERSION" ]; then
	if VERSION="$(git -C "$ROOT" describe --tags --always --dirty 2>/dev/null)"; then
		VERSION="$(echo "$VERSION" | sed 's/^v//')"
	fi
	[ -n "$VERSION" ] || VERSION="0.0.0"
fi

MAINTAINER="${LEAD_MAINTAINER:-andreiganea69@gmail.com}"
WWW="${LEAD_WWW:-https://github.com/Index-s/lead-files-community}"

log() { echo "==> $*"; }
die() { echo "build.sh: ERROR: $*" >&2; exit 1; }

[ "$(uname -s)" = "FreeBSD" ] || die "must run on FreeBSD (produces amd64 ELF). Use the VM or CI."

# --- resolve MariaDB (latest available unless overridden via --mariadb) ------
# Refresh the catalog first so `pkg rquery` sees the newest versions.
[ "$INSTALL_DEPS" -eq 1 ] && pkg update >/dev/null 2>&1 || true
detect_mariadb_server() {
	# Newest mariadb<NNN>-server by semantic version (sort -V handles 10.11 < 11.4 < 11.8).
	pkg rquery '%n %v' 2>/dev/null \
		| awk '$1 ~ /^mariadb[0-9]+-server$/ {print $2, $1}' \
		| sort -V | tail -1 | awk '{print $2}'
}
if [ -z "$MARIADB_PKG" ]; then
	MARIADB_PKG="$(detect_mariadb_server || true)"
	if [ -n "$MARIADB_PKG" ]; then
		log "MariaDB (latest available): $MARIADB_PKG"
	else
		MARIADB_PKG="mariadb114-server"
		log "WARNING: could not query repos for MariaDB; defaulting to ${MARIADB_PKG}"
	fi
fi
# Accept a bare base ("mariadb118") as well as a full "...-server" name.
case "$MARIADB_PKG" in
	*-server) ;;
	mariadb[0-9]*) MARIADB_PKG="${MARIADB_PKG}-server" ;;
esac
MARIADB_CLIENT="${MARIADB_PKG%-server}-client"

# --- 0. build dependencies --------------------------------------------------
# The compile needs the MariaDB client headers (mysql.h); the runtime package
# depends on the server (declared in the manifest below).
BUILD_DEPS="gmake llvm lzo2 png jpeg-turbo cryptopp devil boost-libs ${MARIADB_CLIENT}"
if [ "$INSTALL_DEPS" -eq 1 ]; then
	log "installing build dependencies: $BUILD_DEPS"
	pkg install -y $BUILD_DEPS || die "pkg install of build deps failed"
fi
command -v gmake >/dev/null 2>&1 || die "gmake not found (run with --install-deps or pkg install gmake)"
command -v pkg   >/dev/null 2>&1 || die "pkg not found"

# --- 1. compile -------------------------------------------------------------
if [ "$DO_BUILD" -eq 1 ]; then
	log "building server (gmake) in $SRC ..."
	# `gmake -B` (always-make): the top Makefile's game/db/lib*  targets are not
	# .PHONY and collide with same-named directories, so a plain `gmake` skips
	# them when the directory looks up-to-date, leaving game/db unbuilt.
	( cd "$SRC" && gmake -B ) || die "server build failed"
	# Belt-and-suspenders: build the cores directly from their src dirs (the libs
	# are built above). This is what actually produces game/game and db/db.
	[ -f "${SRC}/game/game" ] || ( cd "$SRC/game/src" && gmake ) || die "game core build failed"
	[ -f "${SRC}/db/db" ]     || ( cd "$SRC/db/src"   && gmake ) || die "db core build failed"
fi
[ -f "${SRC}/game/game" ] || die "missing build output ${SRC}/game/game"
[ -f "${SRC}/db/db" ]     || die "missing build output ${SRC}/db/db"

# Sanity: warn about non-base dynamic deps so the manifest stays honest.
log "runtime shared-library deps of game:"
ldd "${SRC}/game/game" 2>/dev/null | awk '{print "    " $0}' || true

# --- 2. stage ---------------------------------------------------------------
STAGE="$(mktemp -d)"; trap 'rm -rf "$STAGE"' EXIT
P="${STAGE}/usr/local"
log "staging into $STAGE"

install -d "${P}/libexec/lead" "${P}/share/lead/conf" "${P}/share/lead/db-scripts" \
           "${P}/etc/lead/db" "${P}/etc/rc.d"

# binaries
install -m 0755 "${SRC}/game/game" "${P}/libexec/lead/game"
install -m 0755 "${SRC}/db/db"     "${P}/libexec/lead/db"
# helper scripts
install -m 0755 "${PKGDIR}/files/lead-db-setup" "${P}/libexec/lead/lead-db-setup"
install -m 0755 "${PKGDIR}/files/lead-layout"   "${P}/libexec/lead/lead-layout"
# rc.d
install -m 0755 "${PKGDIR}/files/rc.d/lead"     "${P}/etc/rc.d/lead"

# content tree
cp -R "${SVF}/share/data"   "${P}/share/lead/data"
cp -R "${SVF}/share/locale" "${P}/share/lead/locale"
cp -R "${SVF}/share/conf/." "${P}/share/lead/conf/"
install -d "${P}/share/lead/package"          # empty content root (symlink target)
: > "${P}/share/lead/package/.keep"

# db bootstrap data
cp -R "${DBS}/base"       "${P}/share/lead/db-scripts/base"
cp -R "${DBS}/migrations" "${P}/share/lead/db-scripts/migrations"

# per-core configs (@config; copied from the known-good serverfiles)
install -m 0644 "${SVF}/db/conf.txt"               "${P}/etc/lead/db/conf.txt"
for core in auth channel1/game1 channel1/game2 channel99 markserver; do
	install -d "${P}/etc/lead/${core}"
	install -m 0644 "${SVF}/${core}/CONFIG" "${P}/etc/lead/${core}/CONFIG"
done

# normalise perms on copied trees
find "${P}/share/lead" -type d -exec chmod 0755 {} +
find "${P}/share/lead" -type f -exec chmod 0644 {} +

# Drop 32-bit ELF build tools shipped inside the content tree (e.g. the quest
# compiler locale/.../quest/qc). They can't run on the x64-only server and would
# add spurious ":32" shared-library requirements to the package.
log "pruning 32-bit ELF dev tools from the stage ..."
find "${P}" -type f -exec sh -c '
	for f do
		if file -b "$f" 2>/dev/null | grep -q "ELF 32-bit"; then
			echo "    removed $f"; rm -f "$f"
		fi
	done
' _ {} +

# --- 3. plist (auto-generated; mark @config) --------------------------------
PLIST="$(mktemp)"
( cd "$STAGE" && find . \( -type f -o -type l \) | sed 's/^\.//' | sort ) > "$PLIST"
# Prefix config files with @config so admin edits survive upgrades.
TMP_PLIST="$(mktemp)"
while IFS= read -r line; do
	case "$line" in
		/usr/local/etc/lead/*/CONFIG|/usr/local/etc/lead/db/conf.txt)
			echo "@config ${line}" ;;
		*) echo "$line" ;;
	esac
done < "$PLIST" > "$TMP_PLIST"
mv "$TMP_PLIST" "$PLIST"
log "plist entries: $(wc -l < "$PLIST")"

# --- 4. manifest (metadata + deps) ------------------------------------------
META="$(mktemp -d)"; trap 'rm -rf "$STAGE" "$META" "$PLIST"' EXIT

dep_line() { # dep_line <pkgname> <origin>
	_v="$(pkg query '%v' "$1" 2>/dev/null || true)"
	if [ -n "$_v" ]; then
		printf '  %s: { origin: "%s", version: "%s" }\n' "$1" "$2" "$_v"
	else
		printf '  %s: { origin: "%s" }\n' "$1" "$2"
	fi
}
# Runtime deps. Derived from `ldd` of the built game core: it links DevIL
# (libIL) dynamically, which transitively pulls png/jpeg/tiff/jasper/mng/lcms2/
# squish/zstd/etc; plus our own lzo2. `db` is self-contained (static mysqlclient
# + base). mariadb server is needed by the app, its client by lead-db-setup.
DEPS_FILE="$(mktemp)"
{
	dep_line devil graphics/devil
	dep_line lzo2  archivers/lzo2
	dep_line "$MARIADB_PKG" "databases/${MARIADB_PKG}"
} > "$DEPS_FILE"

sed -e "s|%%VERSION%%|${VERSION}|g" \
    -e "s|%%MAINTAINER%%|${MAINTAINER}|g" \
    -e "s|%%WWW%%|${WWW}|g" \
    -e "/%%DEPS%%/r ${DEPS_FILE}" \
    -e "/%%DEPS%%/d" \
    "${PKGDIR}/manifest/+MANIFEST.in" > "${META}/+MANIFEST"
rm -f "$DEPS_FILE"

# Embed lifecycle scripts + install message directly into the manifest as a
# `scripts` object and a `messages` array. This is the ports-proven approach and
# avoids depending on whether `pkg create -m` reads separate +POST_INSTALL files.
# JSON-escape each script body (backslash, quote, tab, newline) so arbitrary
# shell (incl. ${...}) embeds unambiguously.
json_escape() {
	awk 'BEGIN{ORS=""}
	     { s=$0; gsub(/\\/,"\\\\",s); gsub(/"/,"\\\"",s); gsub(/\t/,"\\t",s); print s "\\n" }' "$1"
}
{
	echo "scripts: {"
	printf '  "post-install": "%s",\n'   "$(json_escape "${PKGDIR}/manifest/+POST_INSTALL")"
	printf '  "pre-deinstall": "%s",\n'  "$(json_escape "${PKGDIR}/manifest/+PRE_DEINSTALL")"
	printf '  "post-deinstall": "%s"\n'  "$(json_escape "${PKGDIR}/manifest/+POST_DEINSTALL")"
	echo "}"
	echo "messages: ["
	printf '  { message: "%s" }\n'       "$(json_escape "${PKGDIR}/manifest/+DISPLAY")"
	echo "]"
} >> "${META}/+MANIFEST"

# --- 5. create the package --------------------------------------------------
mkdir -p "$OUTDIR"
log "creating package (version ${VERSION}) ..."
pkg create -o "$OUTDIR" -r "$STAGE" -p "$PLIST" -m "$META" \
	|| die "pkg create failed"

log "done:"
ls -la "$OUTDIR"/lead-server-*.pkg 2>/dev/null || ls -la "$OUTDIR"
