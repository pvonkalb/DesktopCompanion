#include "touch_input.h"

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "board_cyd.h"

namespace touch_input {
namespace {

constexpr int16_t PRESSURE_MIN = 300;   // below this a reading is treated as noise

SPIClass touchSpi(VSPI);   // display uses HSPI, so VSPI is free
XPT2046_Touchscreen ts(board::PIN_TOUCH_CS, board::PIN_TOUCH_IRQ);

}  // namespace

void begin() {
    touchSpi.begin(board::PIN_TOUCH_CLK, board::PIN_TOUCH_MISO,
                   board::PIN_TOUCH_MOSI, board::PIN_TOUCH_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);   // match the display's landscape rotation
}

bool irqActive() {
    return digitalRead(board::PIN_TOUCH_IRQ) == LOW;
}

RawSample readRaw() {
    RawSample s;
    const TS_Point p = ts.getPoint();
    s.x = p.x;
    s.y = p.y;
    s.z = p.z;
    s.touched = p.z >= PRESSURE_MIN;
    return s;
}

}  // namespace touch_input
