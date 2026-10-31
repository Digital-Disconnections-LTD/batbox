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
std::vector<std::string> ripGrep(const std::vector<std::string>& args,
  const std::string& target,std::stop_token stopToken,const RipgrepConfig& config);
} // namespace batbox::filesystem
