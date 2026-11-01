#pragma once
#include "flights/Transcript.hpp"
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::flights {
// Picker rows come from a bounded read of each transcript: the head for the
// first prompt and the last 64 KiB for titles, branch, model and end state.
// The result is a lite log (empty "messages") that Restore loads in full only
// when a row is chosen.
std::optional<Json> scanFlightFileLite(const std::string& path,
  const std::string& flightId,double modifiedMs,PoetryServices& poetry);
// Milliseconds since the epoch for an ISO-8601 UTC timestamp, or nullopt.
std::optional<double> isoTimestampMs(const std::string& value);
std::string relativeTimeText(double thenMs,double nowMs);
std::string shortModelName(const std::string& model);
// Custom title, then AI title, then the first prompt excerpt.
std::string flightListingTitle(const Json& log);
// "title · 2 hours ago · branch · model · 12 messages · interrupted  [slug]"
std::string flightListingLabel(const Json& log,double nowMs);
// The picker's dimmer line under the title:
// "slug · branch · model · 2 hours ago · 12 messages". Parts that are unknown
// are left out.
std::string flightListingDetail(const Json& log,double nowMs);
// How the Flight's last run ended: "ended", "interrupted", "last reply failed",
// or empty when no end marker was saved.
std::string flightListingEndState(const Json& log);
// Lower-cased text a picker filter matches against.
std::string flightSearchText(const Json& log);
// Indices of logs matching a /resume or --resume argument. Exact slug, title
// or Flight id prefix matches win over substring matches.
std::vector<std::size_t> matchFlightLogs(const Json& logs,const std::string& query);
}
