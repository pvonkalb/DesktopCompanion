#pragma once
// USB serial link to the optional Windows companion (docs/protocol.md).
// Non-blocking: loop() drains whatever bytes have arrived and handles complete lines.

#include <stdint.h>

namespace pc_link {

void begin();                 // restore the last calendar from NVS, announce ourselves
void loop(uint32_t nowMs);
bool connected();             // a valid message arrived in the last 15 s
bool everConnected();         // since boot

// Tell the PC something happened (e.g. a timer finished) so it can play a sound /
// show a notification. Fire-and-forget; ignored if no companion is listening.
void notify(const char* kind, const char* title, const char* text);

}  // namespace pc_link
