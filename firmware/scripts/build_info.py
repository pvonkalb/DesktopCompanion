# PlatformIO pre-script: make src/version.cpp recompile on every build so its
# __DATE__/__TIME__ reflect the build actually flashed (other files are cached).
import os
import time

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

path = os.path.join(env.subst("$PROJECT_SRC_DIR"), "version.cpp")  # noqa: F821
if os.path.exists(path):
    now = time.time()
    os.utime(path, (now, now))
