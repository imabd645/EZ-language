#!/usr/bin/env bash
#
# Builds a macOS .pkg installer for EZ that installs the interpreter and its
# standard library together under /usr/local/lib/ez, with a symlink on PATH.
#
# Usage:
#   build_pkg.sh <version> <staged-dir>
#
# <staged-dir> must contain:
#   ez           (the interpreter binary)
#   lib/         (the standard library folder)
#
# Output: ez-v<version>-macos-x64.pkg in the current directory.
#
# Requires: pkgbuild (ships with Xcode Command Line Tools / macOS runners).

set -euo pipefail

VERSION="${1:?Usage: build_pkg.sh <version> <staged-dir>}"
STAGE="${2:?Usage: build_pkg.sh <version> <staged-dir>}"
IDENTIFIER="site.ez-lang.ez"
OUTFILE="ez-v${VERSION}-macos-x64.pkg"

if [ ! -f "$STAGE/ez" ]; then
  echo "error: $STAGE/ez not found" >&2
  exit 1
fi
if [ ! -d "$STAGE/lib" ]; then
  echo "error: $STAGE/lib not found" >&2
  exit 1
fi

INSTALL_ROOT="$(mktemp -d)"
SCRIPTS_DIR="$(mktemp -d)"
trap 'rm -rf "$INSTALL_ROOT" "$SCRIPTS_DIR"' EXIT

mkdir -p "$INSTALL_ROOT/usr/local/lib/ez"

# Interpreter and its standard library live side by side under one
# directory -- `use "db"` etc. resolve relative to the binary's own
# location with no extra configuration needed.
cp "$STAGE/ez" "$INSTALL_ROOT/usr/local/lib/ez/ez"
cp -r "$STAGE/lib" "$INSTALL_ROOT/usr/local/lib/ez/lib"
chmod 0755 "$INSTALL_ROOT/usr/local/lib/ez/ez"

# pkgbuild's --root payload doesn't reliably preserve a symlink created
# ahead of time the way a postinstall script does, so the /usr/local/bin
# symlink is created at install time instead of being staged in the payload.
mkdir -p "$SCRIPTS_DIR"
cat > "$SCRIPTS_DIR/postinstall" <<'EOF'
#!/bin/bash
set -e
mkdir -p /usr/local/bin
ln -sf /usr/local/lib/ez/ez /usr/local/bin/ez
exit 0
EOF
chmod 0755 "$SCRIPTS_DIR/postinstall"

pkgbuild \
  --root "$INSTALL_ROOT" \
  --scripts "$SCRIPTS_DIR" \
  --identifier "$IDENTIFIER" \
  --version "$VERSION" \
  --install-location "/" \
  "$OUTFILE"

echo "Built $OUTFILE"