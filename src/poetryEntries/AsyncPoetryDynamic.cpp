#include "poetryEntries/AsyncSystemPoetryInternal.hpp"

namespace batbox::poetryEntries::async_poetry {
void Run::dynamic(){
  try{
    if(!services.sections)throw std::invalid_argument("Missing system prompt section cache");
    auto self=shared_from_this();
    std::vector<AsyncPoetrySection> dynamic={
      systemPoetrySection("session_guidance",ComputeAsyncSection([self]()->AsyncSectionResult{
        need(self->services.flightSpecificGuidanceSection,"sessionSpecificGuidanceSection");
        return self->services.flightSpecificGuidanceSection(self->enabled,*self->commands);})),
      systemPoetrySection("memory",ComputeAsyncSection([self]()->AsyncSectionResult{
        need(self->services.loadMemoryPoetry,"loadMemoryPrompt");return self->services.loadMemoryPoetry();})),
      systemPoetrySection("ant_model_override",ComputeAsyncSection([self]()->AsyncSectionResult{
        need(self->services.antModelOverrideSection,"antModelOverrideSection");return self->services.antModelOverrideSection();})),
      systemPoetrySection("env_info_simple",ComputeAsyncSection([self]()->AsyncSectionResult{
        return self->services.computeSimpleEnvInfo(self->input.model,*self->input.additionalWorkingDirectories);})),
      systemPoetrySection("language",ComputeAsyncSection([self]()->AsyncSectionResult{
        const auto& value=value::property(*self->settings,"language");
        return getLanguageSection(value.is_string()?std::optional<std::string>(value.get<std::string>()):std::nullopt);})),
      systemPoetrySection("output_style",ComputeAsyncSection([self]()->AsyncSectionResult{return getOutputStyleSection(*self->style);})),
      uncachedSystemPoetrySection("mcp_instructions",ComputeAsyncSection([self]()->AsyncSectionResult{
        need(self->services.mcpInstructionsDeltaEnabled,"mcpInstructionsDeltaEnabled");
        return self->services.mcpInstructionsDeltaEnabled()?SectionValue{}:getMcpInstructionsSection(*self->input.mcpClients);
      }),"MCP servers connect/disconnect between turns"),
      systemPoetrySection("scratchpad",ComputeAsyncSection([self]()->AsyncSectionResult{
        need(self->services.scratchpadInstructions,"scratchpadInstructions");return self->services.scratchpadInstructions();})),
      systemPoetrySection("frc",ComputeAsyncSection([self]()->AsyncSectionResult{
        need(self->services.functionResultClearingSection,"functionResultClearingSection");return self->services.functionResultClearingSection(self->input.model);})),
      systemPoetrySection("summarize_tool_results",ComputeAsyncSection([]()->AsyncSectionResult{return SectionValue(summarize);})),
    };
    if(services.internalUser)dynamic.push_back(systemPoetrySection("numeric_length_anchors",ComputeAsyncSection([]()->AsyncSectionResult{return SectionValue(numericLength);})));
    if(services.tokenBudgetFeature)dynamic.push_back(systemPoetrySection("token_budget",ComputeAsyncSection([]()->AsyncSectionResult{return SectionValue(tokenBudget);})));
    if(services.kairosFeature||services.kairosBriefFeature)
      dynamic.push_back(systemPoetrySection("brief",ComputeAsyncSection([self]()->AsyncSectionResult{
        need(self->services.briefSection,"briefSection");return self->services.briefSection();})));
    services.sections->resolveSystemPoetrySections(dynamic).then(
      [self](const std::vector<SectionValue>* values,const std::exception_ptr& error){
        if(error)self->result.reject(error);else self->finish(*values);
      });
  }catch(...){result.reject(std::current_exception());}
}
}
