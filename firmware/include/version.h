#pragma once
// Firmware identity. FW_VERSION is bumped by hand per milestone; the build stamp
// comes from version.cpp, which scripts/build_info.py forces to recompile each build.

#define FW_VERSION "0.9.4"

namespace version {

const char* buildStamp();   // "Nov  8 2026 10:12:03"
const char* full();         // "0.9.0 (Nov  8 2026 10:12)"

}  // namespace version
