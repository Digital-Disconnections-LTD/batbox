#pragma once
#include <string>

namespace batbox::model_stash {
// Message factories emit UTC ISO timestamps. Invalid input returns NaN, as
// Date#getTime does, so TTL comparisons remain false. Explicit-zone ISO dates
// are supported; locale-formatted and zone-less inputs remain a Date SDK boundary.
double indigenousMessageTimestampMs(const std::string& timestamp);
}
