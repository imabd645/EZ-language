#!/usr/bin/env bash
#
# Builds a .deb package for EZ that installs the interpreter and its
# standard library together under /usr/lib/ez, with a symlink on PATH.
#
# Usage:
#   build_deb.sh <version> <staged-dir>
#
# <staged-dir> must contain:
#   ez           (the interpreter binary)
#   lib/         (the standard library folder)
#
# Output: ez-v<version>-linux-x64.deb in the current directory.

set -euo pipefail

VERSION="${1:?Usage: build_deb.sh <version> <staged-dir>}"
STAGE="${2:?Usage: build_deb.sh <version> <staged-dir>}"
ARCH="amd64"
PKGNAME="ez"
OUTFILE="ez-v${VERSION}-linux-x64.deb"

if [ ! -f "$STAGE/ez" ]; then
  echo "error: $STAGE/ez not found" >&2
  exit 1
fi
if [ ! -d "$STAGE/lib" ]; then
  echo "error: $STAGE/lib not found" >&2
  exit 1
fi

PKGROOT="$(mktemp -d)"
trap 'rm -rf "$PKGROOT"' EXIT

mkdir -p "$PKGROOT/DEBIAN"
mkdir -p "$PKGROOT/usr/lib/ez"
mkdir -p "$PKGROOT/usr/bin"

# Interpreter and its standard library live side by side, exactly as they
# do in a source checkout -- `use "db"` etc. resolve relative to the
# binary's own directory with no extra configuration needed.
cp "$STAGE/ez" "$PKGROOT/usr/lib/ez/ez"
cp -r "$STAGE/lib" "$PKGROOT/usr/lib/ez/lib"
chmod 0755 "$PKGROOT/usr/lib/ez/ez"

# Symlink puts `ez` on PATH without duplicating or relocating the binary.
ln -s /usr/lib/ez/ez "$PKGROOT/usr/bin/ez"

cat > "$PKGROOT/DEBIAN/control" <<EOF
Package: ${PKGNAME}
Version: ${VERSION}
Section: devel
Priority: optional
Architecture: ${ARCH}
Maintainer: Abdullah Masood <maintainer@ez-lang.site>
Homepage: https://ez-lang.site
Description: EZ programming language interpreter
 A fast, modern scripting language with built-in GUI, SQLite, async/await,
 OOP, and a standalone bundler. This package installs the interpreter and
 its bundled standard library together under /usr/lib/ez.
EOF

# Recommended for reproducible/clean-looking packages; keeps generated
# files from carrying the build runner's uid/gid.
dpkg-deb --build --root-owner-group "$PKGROOT" "$OUTFILE"

echo "Built $OUTFILE"