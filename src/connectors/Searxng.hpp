#pragma once
#include "runtime/JsonValue.hpp"
#include "connectors/Abort.hpp"
#include <chrono>
#include <stdexcept>
#include <string>
namespace batbox {
// Why a SearXNG reply is not the JSON search answer Batbox reads.
struct SearxngProblem {
  enum class Kind {JsonOff,HttpStatus,NotJson,NoResults,NoUrl};
  Kind kind=Kind::HttpStatus;
  // What was expected and what came back (status, content type, start of the body or the missing field).
  std::string detail;
  std::string fix;
};
class SearxngResponseError : public std::runtime_error {
public:
  SearxngResponseError(const std::string& endpoint, SearxngProblem problem);
  SearxngProblem problem;
};
// The shape Batbox relies on: HTTP 200 and a JSON object whose "results" is an
// array of objects with a string "url". title, content, engine, publishedDate,
// number_of_results and suggestions are optional. Rows without a url are
// dropped; when no row has one the format has changed and this throws.
// Returns the rows that have a url.
Json searxngResults(const std::string& endpoint, long status, const std::string& contentType,
  const std::string& body);
// SearXNG search connector with cancellation and request timeouts.
class Searxng {
  std::string endpoint;
public:
  explicit Searxng(std::string endpoint = "http://127.0.0.1:8888/search");
  Json search(const Json& input, const std::string& gadgetUseId,
    std::chrono::milliseconds timeout = std::chrono::minutes(2),std::stop_token signal={}) const;
};
} // namespace batbox
