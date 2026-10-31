#include "betas/WireValues.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto input=value::parse(line);
    const auto h=betas::makeProviderWireHeaders(
      input.value("connectorText",false),input.value("internal",false));
    Json out={{"core",h.core},{"internal",h.internal},{"oauth",h.oauth},
      {"context1m",h.context1m},
      {"interleavedThinking",h.interleavedThinking},
      {"redactThinking",h.redactThinking},
      {"summarizeConnectorText",h.summarizeConnectorText},
      {"contextManagement",h.contextManagement},
      {"structuredOutputs",h.structuredOutputs},
      {"tokenEfficientTools",h.tokenEfficientGadgets},
      {"webSearch",h.webSearch},
      {"promptCachingScope",h.poetryStashingScope},
      {"toolSearchFirstParty",h.gadgetSearchFirstParty},
      {"toolSearchThirdParty",h.gadgetSearchThirdParty},
      {"bedrockExtraBodyParams",h.bedrockExtraBodyParams}};
    std::cout<<value::stringify(out)<<'\n';
  }
}
