# PlatformIO post-script: make esptool retry connecting forever.
# This CYD's auto-reset can't pull IO0 low, so the user must hold BOOT while
# esptool shows "Connecting...". Without this, esptool gives up after ~10 s.
# See docs/hardware.md -> Troubleshooting USB.
Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

env.Prepend(UPLOADERFLAGS=["--connect-attempts", "0"])  # noqa: F821
