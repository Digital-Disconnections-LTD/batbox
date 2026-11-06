#pragma once
#include <curl/curl.h>
#include <stop_token>

namespace batbox::transport {
// Runs one transfer on its own multi handle so a stop request wakes it at
// once, even while no bytes arrive. Returns CURLE_ABORTED_BY_CALLBACK when
// stopped; callers check the token before reading the result.
CURLcode performStoppable(CURL* easy, std::stop_token stop);
} // namespace batbox::transport
