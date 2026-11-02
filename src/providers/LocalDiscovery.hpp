#pragma once
#include "mcp/http/Http.hpp"
#include <chrono>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace batbox::providers {
// A model server answering on this machine.
struct LocalServer {
  std::string preset,name,baseUrl;
  int port=0;
  std::vector<std::string> models;
  // Context length per model when the server reports it.
  std::map<std::string,std::size_t> contexts;
  // False when the port answered like a model server but did not identify
  // itself as the preset it was probed for.
  bool confirmed=true;
};
struct ProbeTarget {
  std::string preset,name,host="127.0.0.1";
  int port=0;
};
// Ollama 11434, LM Studio 1234, llama-server 9931 and 8080, vLLM 8000.
std::vector<ProbeTarget> defaultProbeTargets();
// Probes every target in parallel, each request bounded by timeout, and
// returns the servers that answered, in target order. Never throws.
std::vector<LocalServer> discoverLocalServers(
  const std::vector<ProbeTarget>& targets=defaultProbeTargets(),
  std::chrono::milliseconds timeout=std::chrono::milliseconds(300),
  mcp::http::Fetch fetch=mcp::http::fetch);
// "Ollama with 3 models" / "llama.cpp on :8080 with 1 model"
std::string describeLocalServer(const LocalServer& server);
} // namespace batbox::providers
