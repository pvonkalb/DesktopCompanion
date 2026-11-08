# PlatformIO post-script: make esptool retry connecting forever.
# This CYD's auto-reset can't pull IO0 low, so the user must hold BOOT while
# esptool shows "Connecting...". Without this, esptool gives up after ~10 s.
# USB (esptool) uploads only - OTA uploads (espota) take different arguments.
# See docs/hardware.md -> Troubleshooting USB.
Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

if env.subst("$UPLOAD_PROTOCOL") in ("", "esptool"):  # noqa: F821
    env.Prepend(UPLOADERFLAGS=["--connect-attempts", "0"])  # noqa: F821
