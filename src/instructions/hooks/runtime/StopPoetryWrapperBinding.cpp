#include "instructions/hooks/runtime/StopPoetryWrappers.hpp"
#include "messages/Context.hpp"
#include "messages/TextContent.hpp"

namespace batbox::instructions::hooks::runtime {
StopPoetryServices bindStopPoetryServices(PoetryGadgetServices poetry,
  std::function<std::string(const std::string&)> getBatTranscriptPath){
  return {std::move(poetry),messages::getLastAssistantMessage,
    messages::extractTextContent,std::move(getBatTranscriptPath)};
}
} // namespace batbox::instructions::hooks::runtime
