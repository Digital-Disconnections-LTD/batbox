#pragma once
#include "terminal_output/Output.hpp"
#include <functional>
#include <optional>

namespace batbox::terminal_output {
struct BidiEnvironment {
  std::string platform;
  std::optional<std::string> wtFlight,termProgram;
};
using EmbeddingLevels=std::vector<double>;
using LevelsProvider=std::function<EmbeddingLevels(const std::string&)>;
using BidiFactory=std::function<LevelsProvider()>;
bool hasRTLCharacters(const std::string& text);
class BidiReorder {
public:
  BidiReorder(std::function<BidiEnvironment()> environment,
    BidiFactory factory);
  ClusteredCharsRef reorder(const ClusteredCharsRef& characters);
private:
  bool needsBidi();
  LevelsProvider& getBidi();
  std::function<BidiEnvironment()> environment_;
  BidiFactory factory_;
  std::optional<bool> needsSoftwareBidi_;
  LevelsProvider bidi_;
};
BidiEnvironment indigenousBidiEnvironment();
BidiFactory indigenousBidiFactory();
} // namespace batbox::terminal_output
