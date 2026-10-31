#include "poetryEntries/PoetryBlocks.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::poetryEntries {
namespace {
std::string stringValue(const Json& value){
  return value.is_string()?value.get<std::string>():value::stringify(value);
}
}
std::vector<std::string> prependBullets(const Json& items){
  std::vector<std::string> result;
  for(const auto& item:items){
    if(item.is_array()){
      for(const auto& subitem:item)
        result.push_back("  - "+stringValue(subitem));
    }else result.push_back(" - "+stringValue(item));
  }
  return result;
}
std::optional<std::string> getLanguageSection(
  const std::optional<std::string>& language){
  if(!language||language->empty())return std::nullopt;
  return "# Language\nAlways respond in "+*language+". Use "+*language+
    " for all explanations, comments, and communications with the user. "
    "Technical terms and code identifiers should remain in their original form.";
}
std::optional<std::string> getOutputStyleSection(const Json& config){
  if(config.is_null())return std::nullopt;
  return "# Output Style: "+stringValue(value::property(config,"name"))+"\n"+
    stringValue(value::property(config,"prompt"));
}
std::optional<std::string> getMcpInstructions(const Json& clients){
  std::vector<std::string> blocks;
  if(clients.is_array())for(const auto& client:clients){
    if(value::property(client,"type")!="connected")continue;
    const auto& instruction=value::property(client,"instructions");
    if(!value::truthy(instruction))continue;
    blocks.push_back("## "+stringValue(value::property(client,"name"))+"\n"+
      stringValue(instruction));
  }
  if(blocks.empty())return std::nullopt;
  std::string joined;
  for(std::size_t i=0;i<blocks.size();++i){
    if(i)joined+="\n\n";
    joined+=blocks[i];
  }
  return "# MCP Server Instructions\n\nThe following MCP servers have "
    "provided instructions for how to use their tools and resources:\n\n"+
    joined;
}
std::optional<std::string> getMcpInstructionsSection(const Json& clients){
  if(!clients.is_array()||clients.empty())return std::nullopt;
  return getMcpInstructions(clients);
}
std::string getSimpleIntroSection(const Json& outputStyleConfig,
  const std::string& cyberRiskInstruction){
  const std::string phrase=outputStyleConfig.is_null()
    ?"with software engineering tasks."
    :"according to your \"Output Style\" below, which describes how you should respond to user queries.";
  return "\nYou are an interactive agent that helps users "+phrase+
    " Use the instructions below and the tools available to you to assist the user.\n\n"+
    cyberRiskInstruction+"\nIMPORTANT: You must NEVER generate or guess URLs for "
    "the user unless you are confident that the URLs are for helping the user "
    "with programming. You may use URLs provided by the user in their messages "
    "or local files.";
}
}
