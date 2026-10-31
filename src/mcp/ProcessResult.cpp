#include "mcp/ProcessResult.hpp"
#include "mcp/OutputStorage.hpp"
#include "runtime/JsonValue.hpp"
#include <cctype>
#include <stdexcept>

namespace batbox::mcp {
namespace {
bool envDefinedFalsy(const std::optional<std::string>& value){
  if(!value||value->empty())return false;
  auto normalized=value::trim(*value);
  for(char& c:normalized)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return normalized=="0"||normalized=="false"||normalized=="no"||normalized=="off";
}
bool containsImages(const Json& content){
  if(!content.is_array())return false;
  for(const auto& block:content)if(value::property(block,"type")=="image")return true;
  return false;
}
std::string grouped(std::size_t value){
  auto source=std::to_string(value);
  for(std::size_t i=source.size();i>3;i-=3)source.insert(i-3,",");
  return source;
}
std::string pretty(const Json& value,int depth=0){
  if(!value.is_array()&&!value.is_object())return value::stringify(value);
  if(value.empty())return value.is_array()?"[]":"{}";
  const auto indent=[](int amount){return std::string(amount*2,' ');};
  const bool array=value.is_array();
  std::string out=array?"[\n":"{\n";
  if(array){
    for(std::size_t i=0;i<value.size();++i){
      if(i)out+=",\n";
      out+=indent(depth+1)+pretty(value[i],depth+1);
    }
  }else{
    const auto keys=jsOwnKeys(value);
    for(std::size_t i=0;i<keys.size();++i){
      if(i)out+=",\n";
      out+=indent(depth+1)+value::quote(keys[i])+": "+pretty(value.at(keys[i]),depth+1);
    }
  }
  out+="\n"+indent(depth)+(array?"]":"}");
  return out;
}
}
Json processMCPResult(const Json& raw,const std::string& gadget,
  const std::string& serverName,const ProcessResultServices& services){
  const auto transformed=transformMCPResult(raw,gadget,serverName,services.result);
  const auto& content=transformed.content;
  if(serverName==services.ideServerName)return content;
  if(!mcpContentNeedsTruncation(content,services.validation))return content;
  if(envDefinedFalsy(services.enableLargeOutputFiles))
    return truncateMcpContentIfNeeded(content,services.validation);
  if(!value::truthy(content))return content;
  if(containsImages(content))return truncateMcpContentIfNeeded(content,services.validation);
  if(!services.persistGadgetResult||!services.nowMs)
    throw std::invalid_argument("Missing MCP large-result storage or clock import");
  const std::string id="mcp-"+normalizeNameForMCP(serverName,services.result.hostedServerPrefix)+
    "-"+normalizeNameForMCP(gadget,services.result.hostedServerPrefix)+
    "-"+std::to_string(services.nowMs());
  const auto contentStr=content.is_string()?content.get<std::string>():pretty(content);
  const auto persisted=services.persistGadgetResult(contentStr,id);
  if(persisted.is_object()&&persisted.contains("error"))
    return "Error: result ("+grouped(value::stringLength(contentStr))+
      " characters) exceeds maximum allowed tokens. Failed to save output to file: "+
      jsStringValue(persisted.at("error"))+
      ". If this MCP server provides pagination or filtering tools, use them to retrieve specific portions of the data.";
  if(!persisted.is_object()||!persisted.contains("filepath")||!persisted.contains("originalSize"))
    throw std::invalid_argument("MCP tool-result storage returned invalid result");
  return getLargeOutputInstructions(persisted.at("filepath").get<std::string>(),
    persisted.at("originalSize").get<double>(),
    getFormatDescription(transformed.type,transformed.schema));
}
void bindIndigenousProcessResult(Services& gadgetServices,const ProcessResultServices& services){
  gadgetServices.processResult=[services](const Json& raw,const std::string& gadget,
      const std::string& serverName){
    return processMCPResult(raw,gadget,serverName,services);};
}
} // namespace batbox::mcp
