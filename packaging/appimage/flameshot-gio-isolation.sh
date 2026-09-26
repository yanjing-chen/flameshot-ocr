#!/bin/sh
# Do not load host GIO/GVFS modules against the GLib bundled by the AppImage.
# Ubuntu releases newer than the build image may expose incompatible symbols.
unset GIO_EXTRA_MODULES
if [ -n "${APPDIR:-}" ]; then
    export GIO_MODULE_DIR="$APPDIR/usr/lib/gio/modules"
fi
