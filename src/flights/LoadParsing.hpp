#pragma once
#include "flights/Load.hpp"
namespace batbox::flights {
// Shared parsing stages. The async reader supplies buffers after the
// corresponding I/O completes; the synchronous reader keeps its blocking contract.
void parseLoadedTranscriptBuffers(const std::string& buffer,
  const std::vector<std::string>& metadata,LoadedTranscript& output);
void finishLoadedTranscript(LoadedTranscript& output,bool pruneLeaves);
}
