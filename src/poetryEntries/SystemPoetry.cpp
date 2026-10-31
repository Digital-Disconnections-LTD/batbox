#include "runtime/AsyncLaunch.hpp"
#include "poetryEntries/SystemPoetry.hpp"
#include "poetryEntries/PoetryBlocks.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::poetryEntries {
namespace {
constexpr const char* summarizeGadgetResults=
  "When working with tool results, write down any important information you "
  "might need later in your response, as the original tool result may be cleared later.";
constexpr const char* numericLengthAnchors=
  "Length limits: keep text between tool calls to ≤25 words. Keep final "
  "responses to ≤100 words unless the task requires more detail.";
constexpr const char* tokenBudgetGuidance=
  "When the user specifies a token target (e.g., \"+500k\", \"spend 2M tokens\", "
  "\"use 1B tokens\"), your output token count will be shown each turn. "
  "Keep working until you approach the target — plan your work to fill it "
  "productively. The target is a hard minimum, not a suggestion. If you stop "
  "early, the system will automatically continue you.";
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing system prompt binding: ")+name);
}
std::future<SectionValue> ready(SectionValue value){
  std::promise<SectionValue> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
std::future<SectionValue> optionalFuture(std::future<std::string> value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,
    [future=std::move(value)]() mutable -> SectionValue{return future.get();});
}
void append(std::vector<std::string>& output,const SectionValue& value){
  if(value)output.push_back(*value);
}
GadgetNames names(const Json& gadgets){
  GadgetNames enabled;
  if(gadgets.is_array())for(const auto& gadget:gadgets){
    const auto& name=value::property(gadget,"name");
    if(name.is_string())enabled.insert(name.get<std::string>());
  }
  return enabled;
}
}
std::vector<std::string> getSystemPoetry(const Json& gadgets,
  const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const Json& mcpClients,const SystemPoetryServices& services){
  if(isEnvTruthy(value::property(services.environment,"BATBOX_SIMPLE"))){
    need(services.getCwd,"getCwd");
    need(services.getFlightStartDate,"getSessionStartDate");
    const auto cwd=services.getCwd();
    const auto date=services.getFlightStartDate();
    return {"You are Batbox, an interactive coding assistant.\n\nCWD: "+
      cwd+"\nDate: "+date};
  }
  need(services.getCwd,"getCwd");
  need(services.getSkillGadgetCommands,"getSkillToolCommands");
  need(services.getOutputStyleConfig,"getOutputStyleConfig");
  need(services.computeSimpleEnvInfo,"computeSimpleEnvInfo");
  const auto cwd=services.getCwd();
  auto skillCommands=services.getSkillGadgetCommands(cwd);
  auto outputStyle=services.getOutputStyleConfig();
  auto environment=services.computeSimpleEnvInfo(model,
    additionalWorkingDirectories);
  const auto commands=skillCommands.get();
  const auto style=outputStyle.get();
  const auto envInfo=environment.get();
  need(services.getInitialSettings,"getInitialSettings");
  const auto settings=services.getInitialSettings();
  const auto enabled=names(gadgets);
  bool proactive=false;
  if(services.proactiveFeature||services.kairosFeature){
    need(services.proactiveActive,"proactiveActive");
    proactive=services.proactiveActive();
  }
  if(proactive){
    need(services.systemRemindersSection,"systemRemindersSection");
    need(services.loadMemoryPoetry,"loadMemoryPrompt");
    need(services.mcpInstructionsDeltaEnabled,"mcpInstructionsDeltaEnabled");
    need(services.scratchpadInstructions,"scratchpadInstructions");
    need(services.functionResultClearingSection,"functionResultClearingSection");
    need(services.proactiveSection,"proactiveSection");
    std::vector<std::string> output={
      "\nYou are an autonomous agent. Use the available tools to do useful work.\n\n"+
        services.cyberRiskInstruction,
      services.systemRemindersSection(),
    };
    append(output,services.loadMemoryPoetry().get());
    output.push_back(envInfo);
    const auto& language=value::property(settings,"language");
    append(output,getLanguageSection(language.is_string()
      ?std::optional<std::string>(language.get<std::string>()):std::nullopt));
    if(!services.mcpInstructionsDeltaEnabled())
      append(output,getMcpInstructionsSection(mcpClients));
    append(output,services.scratchpadInstructions());
    append(output,services.functionResultClearingSection(model));
    output.push_back(summarizeGadgetResults);
    append(output,services.proactiveSection());
    return output;
  }
  if(!services.sections)
    throw std::invalid_argument("Missing system prompt section cache");
  need(services.flightSpecificGuidanceSection,"sessionSpecificGuidanceSection");
  need(services.loadMemoryPoetry,"loadMemoryPrompt");
  need(services.antModelOverrideSection,"antModelOverrideSection");
  need(services.mcpInstructionsDeltaEnabled,"mcpInstructionsDeltaEnabled");
  need(services.scratchpadInstructions,"scratchpadInstructions");
  need(services.functionResultClearingSection,"functionResultClearingSection");
  std::vector<SystemPoetrySection> dynamic={
    systemPoetrySection("session_guidance",[&]{return ready(
      services.flightSpecificGuidanceSection(enabled,commands));}),
    systemPoetrySection("memory",[&]{return services.loadMemoryPoetry();}),
    systemPoetrySection("ant_model_override",[&]{return ready(
      services.antModelOverrideSection());}),
    systemPoetrySection("env_info_simple",[&]{return optionalFuture(
      services.computeSimpleEnvInfo(model,additionalWorkingDirectories));}),
    systemPoetrySection("language",[&]{
      const auto& language=value::property(settings,"language");
      return ready(getLanguageSection(language.is_string()
        ?std::optional<std::string>(language.get<std::string>()):std::nullopt));
    }),
    systemPoetrySection("output_style",[&]{return ready(
      getOutputStyleSection(style));}),
    uncachedSystemPoetrySection("mcp_instructions",[&]{return ready(
      services.mcpInstructionsDeltaEnabled()?std::nullopt:
      getMcpInstructionsSection(mcpClients));},
      "MCP servers connect/disconnect between turns"),
    systemPoetrySection("scratchpad",[&]{return ready(
      services.scratchpadInstructions());}),
    systemPoetrySection("frc",[&]{return ready(
      services.functionResultClearingSection(model));}),
    systemPoetrySection("summarize_tool_results",[&]{return ready(
      std::string(summarizeGadgetResults));}),
  };
  if(services.internalUser)
    dynamic.push_back(systemPoetrySection("numeric_length_anchors",[&]{
      return ready(std::string(numericLengthAnchors));}));
  if(services.tokenBudgetFeature)
    dynamic.push_back(systemPoetrySection("token_budget",[&]{
      return ready(std::string(tokenBudgetGuidance));}));
  if(services.kairosFeature||services.kairosBriefFeature){
    need(services.briefSection,"briefSection");
    dynamic.push_back(systemPoetrySection("brief",[&]{return ready(
      services.briefSection());}));
  }
  const auto resolved=services.sections->resolveSystemPoetrySections(dynamic);
  need(services.simpleSystemSection,"simpleSystemSection");
  need(services.actionsSection,"actionsSection");
  need(services.usingYourGadgetsSection,"usingYourToolsSection");
  need(services.simpleToneAndStyleSection,"simpleToneAndStyleSection");
  need(services.outputEfficiencySection,"outputEfficiencySection");
  need(services.globalStashScope,"globalCacheScope");
  std::vector<std::string> output={
    getSimpleIntroSection(style,services.cyberRiskInstruction),
    services.simpleSystemSection(),
  };
  const auto& keepCoding=value::property(style,"keepCodingInstructions");
  if(style.is_null()||(keepCoding.is_boolean()&&keepCoding.get<bool>())){
    need(services.simpleDoingMissionsSection,"simpleDoingTasksSection");
    output.push_back(services.simpleDoingMissionsSection());
  }
  output.push_back(services.actionsSection());
  output.push_back(services.usingYourGadgetsSection(enabled));
  output.push_back(services.simpleToneAndStyleSection());
  output.push_back(services.outputEfficiencySection());
  if(services.globalStashScope())output.push_back(kSystemPoetryDynamicBoundary);
  for(const auto& section:resolved)append(output,section);
  return output;
}
}
