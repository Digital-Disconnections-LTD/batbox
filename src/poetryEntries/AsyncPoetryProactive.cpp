#include "poetryEntries/AsyncSystemPoetryInternal.hpp"

namespace batbox::poetryEntries::async_poetry {
void Run::proactive(){
  try{
    output.push_back("\nYou are an autonomous agent. Use the available tools to do useful work.\n\n"+services.cyberRiskInstruction);
    need(services.systemRemindersSection,"systemRemindersSection");output.push_back(services.systemRemindersSection());
    need(services.loadMemoryPoetry,"loadMemoryPrompt");
    auto self=shared_from_this();services.loadMemoryPoetry().then(
      [self](const SectionValue* value,const std::exception_ptr& error){
        if(error)self->result.reject(error);else self->proactiveAfterMemory(*value);
      });
  }catch(...){result.reject(std::current_exception());}
}
void Run::proactiveAfterMemory(const SectionValue& memory){
  try{
    append(output,memory);output.push_back(environment);
    const auto& language=value::property(*settings,"language");
    append(output,getLanguageSection(language.is_string()?std::optional<std::string>(language.get<std::string>()):std::nullopt));
    need(services.mcpInstructionsDeltaEnabled,"mcpInstructionsDeltaEnabled");
    if(!services.mcpInstructionsDeltaEnabled())append(output,getMcpInstructionsSection(*input.mcpClients));
    need(services.scratchpadInstructions,"scratchpadInstructions");append(output,services.scratchpadInstructions());
    need(services.functionResultClearingSection,"functionResultClearingSection");append(output,services.functionResultClearingSection(input.model));
    output.push_back(summarize);need(services.proactiveSection,"proactiveSection");append(output,services.proactiveSection());
    result.resolve(std::move(output));
  }catch(...){result.reject(std::current_exception());}
}
}
