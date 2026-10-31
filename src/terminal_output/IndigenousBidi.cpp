#include "terminal_output/Bidi.hpp"
#include "terminal_output/BidiEmbeddingState.hpp"
#include <cstdlib>

namespace batbox::terminal_output {
BidiEnvironment indigenousBidiEnvironment(){
  BidiEnvironment env;
#ifdef _WIN32
  env.platform="win32";
#elif defined(__APPLE__)
  env.platform="darwin";
#elif defined(__FreeBSD__)
  env.platform="freebsd";
#else
  env.platform="linux";
#endif
  if(const auto value=std::getenv("WT_SESSION"))env.wtFlight=value;
  if(const auto value=std::getenv("TERM_PROGRAM"))env.termProgram=value;
  return env;
}
BidiFactory indigenousBidiFactory(){
  return []()->LevelsProvider{
    return [](const std::string& text)->EmbeddingLevels{
      BidiEmbeddingState state(text);
      return state.compute();
    };
  };
}
} // namespace batbox::terminal_output
