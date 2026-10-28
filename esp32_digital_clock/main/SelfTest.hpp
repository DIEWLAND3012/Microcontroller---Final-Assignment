// SelfTest.hpp - On-target spiral unit tests (results printed to the serial monitor)
#pragma once
#include "SSD1306.hpp"

namespace Clock {

// Returns the number of failed assertions (0 = all passed).
// The display is only used as a scratch frame buffer and is cleared afterwards.
int runSelfTests(SSD1306& scratchDisplay);

}  // namespace Clock
