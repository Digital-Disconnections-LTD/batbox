#include "bats/BatResultBindings.hpp"
#include "messages/Context.hpp"
#include "messages/TextContent.hpp"
#include "runtime/Platform.hpp"
#include "utils/tokens.hpp"

namespace batbox::bats {
BatResultImports indigenousBatResultImports(){
  BatResultImports imports;
  imports.getLastAssistantMessage=messages::getLastAssistantMessage;
  imports.extractTextContent=messages::extractTextContent;
  imports.getTokenCountFromUsage=batbox::getTokenCountFromUsage;
  imports.nowMillis=platform::nowMillis;
  return imports;
}
}
