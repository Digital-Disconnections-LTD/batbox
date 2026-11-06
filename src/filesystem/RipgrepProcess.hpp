#pragma once
#include <cstddef>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

namespace batbox::filesystem {
struct RipgrepConfig {
  std::string command="rg";
  std::vector<std::string> prefixArgs;
  int timeoutMs=20000;
  int timeoutDisplaySeconds=20;
  std::size_t maxBufferBytes=20000000;
};
struct RipgrepTimeoutError : std::runtime_error {
  std::vector<std::string> partialResults;
  RipgrepTimeoutError(const std::string& message,std::vector<std::string> partial)
    :std::runtime_error(message),partialResults(std::move(partial)){}
};
struct RipgrepLines {
  std::vector<std::string> lines;
  // Set when lines may be missing: the model-facing reason, such as a
  // timeout, the output limit or paths ripgrep could not search.
  std::string incomplete;
  RipgrepLines()=default;
  RipgrepLines(std::vector<std::string> values):lines(std::move(values)){}
};
RipgrepLines ripGrep(const std::vector<std::string>& args,
  const std::string& target,std::stop_token stopToken,const RipgrepConfig& config);
// Shared by the platform process runners: classify a run that did not end
// with exit 0 or 1 (after any retry) and return its lines or throw.
struct RipgrepOutcome {
  std::string out,err;
  int exitCode=0;
  bool timedOut=false,aborted=false,overflow=false;
};
RipgrepLines finishRipgrep(const RipgrepOutcome& outcome,const RipgrepConfig& config);
std::vector<std::string> parseRipgrepLines(const std::string& output,bool dropLast);
} // namespace batbox::filesystem
