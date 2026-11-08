#pragma once
// USB serial link to the optional Windows companion (docs/protocol.md).
// Non-blocking: loop() drains whatever bytes have arrived and handles complete lines.

#include <stdint.h>

namespace pc_link {

void begin();                 // restore the last calendar from NVS, announce ourselves
void loop(uint32_t nowMs);
bool connected();             // a valid message arrived in the last 15 s
bool everConnected();         // since boot

}  // namespace pc_link
