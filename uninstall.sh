#!/bin/bash
#Author: Michail Vourlakos
#Summary: Uninstallation script for Latte Dock Panel

# `cmake --install` writes install_manifest.txt into whichever tree performed the install,
# so the manifest lives at the repo root for an in-source build and in the build dir
# otherwise. This used to be pinned to build/, which meant an in-source install was
# uninstalled against whatever stale list an old out-of-source tree had left behind.
REPO="$(cd "$(dirname "$0")" && pwd)"
BUILD="${BUILD:-$REPO}"
MANIFEST="${MANIFEST:-$BUILD/install_manifest.txt}"

if [ ! -f "$MANIFEST" ]; then
   echo "No install manifest at $MANIFEST - nothing has been uninstalled." >&2
   echo "Set BUILD=<build-dir> or MANIFEST=<file> to point at the tree you installed from." >&2
   exit 1
fi

echo "Removing the files listed in $MANIFEST..."
sudo xargs -d '\n' rm < "$MANIFEST"
