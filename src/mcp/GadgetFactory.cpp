#include "runtime/AsyncLaunch.hpp"
#include "mcp/Mcp.hpp"
#include "mcp/Call.hpp"
#include "mcp/Collapse.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
#include <stdexcept>

namespace batbox::mcp {
namespace {
template<class T> std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
bool isJsSpace(char16_t c){
  return (c>=0x09&&c<=0x0d)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||c==0x202f||
    c==0x205f||c==0x3000||c==0xfeff;
}
std::optional<std::string> searchHint(const Json& meta,const std::string& key){
  if(key.empty())return std::nullopt;
  const auto& raw=value::property(meta,key);
  if(!raw.is_string())return std::nullopt;
  std::u16string collapsed;bool inSpace=false;
  for(const char16_t unit:value::utf16CodeUnits(raw.get<std::string>())){
    if(isJsSpace(unit)){
      if(!inSpace)collapsed+=u' ';
      inSpace=true;
    }else {collapsed+=unit;inSpace=false;}
  }
  const auto trimmed=value::trim(value::fromUtf16(collapsed));
  return trimmed.empty()?std::nullopt:std::optional<std::string>(trimmed);
}
std::string gadgetUseId(const Json& parentMessage){
  const auto& content=value::property(value::property(parentMessage,"message"),"content");
  if(!content.is_array()||content.empty())return "";
  const auto& first=content[0];
  if(value::property(first,"type")!="tool_use")return "";
  const auto& id=value::property(first,"id");
  return id.is_string()?id.get<std::string>():"";
}
bool included(const gadgets::Gadget& gadget){
  return !gadget.name.starts_with("mcp__ide__")||gadget.name=="mcp__ide__executeCode"||
    gadget.name=="mcp__ide__getDiagnostics";
}
bool annotation(const Json& gadget,const char* key){
  return value::truthy(value::property(value::property(gadget,"annotations"),key));
}
Json callResultAsJson(const GadgetCallResult& result){
  Json raw={{"content",result.content}};
  if(!result.meta.is_null())raw["_meta"]=result.meta;
  if(!result.structuredContent.is_null())raw["structuredContent"]=result.structuredContent;
  return raw;
}
}
std::vector<gadgets::Gadget> fetchGadgetsForClient(const std::shared_ptr<Connection>& connection,
  const Services& services){
  if(!connection||!connection->connected||!connection->hasGadgetCapability)return {};
  if(!connection->listGadgets||!connection->callGadget||!connection->ensureConnected||
     !services.processResult||
     !services.nowMs||!services.isOutputLineTruncated||
     !services.renderGadgetUseMessage||!services.renderGadgetResultMessage||
     !services.renderGadgetUseProgressMessage)
    throw std::invalid_argument("Missing MCP tool construction imports");
  try{
    const Json list=connection->listGadgets({});
    const auto& gadgets=value::property(list,"tools");
    if(!gadgets.is_array())return {};
    const auto sanitized=services.recursivelySanitizeUnicode
      ?services.recursivelySanitizeUnicode(gadgets):mcp::recursivelySanitizeUnicode(gadgets);
    if(!sanitized.is_array())throw std::invalid_argument("MCP sanitizer returned non-array tools");
    std::vector<gadgets::Gadget> result;
    result.reserve(sanitized.size());
    for(const auto& raw:sanitized){
      if(!raw.is_object()||!value::property(raw,"name").is_string())continue;
      const auto serverName=connection->name;
      const auto sourceName=raw.at("name").get<std::string>();
      const auto fullyQualified=buildMcpGadgetName(serverName,sourceName,
        services.wire.hostedServerPrefix);
      const auto& descriptionField=value::property(raw,"description");
      const auto description=descriptionField.is_string()?descriptionField.get<std::string>():"";
      const auto& metadata=value::property(raw,"_meta");
      const auto& annotations=value::property(raw,"annotations");
      gadgets::Gadget gadget;
      gadget.name=connection->skipPrefix?sourceName:fullyQualified;
      gadget.isMcp=true;gadget.maxResultSizeChars=100000;
      gadget.mcpInfo={{"serverName",serverName},{"toolName",sourceName}};
      gadget.searchHint=searchHint(metadata,services.wire.searchHintMetaKey);
      gadget.alwaysLoad=!services.wire.alwaysLoadMetaKey.empty()&&
        value::property(metadata,services.wire.alwaysLoadMetaKey)==true;
      gadget.inputSchema={{"type","object"},{"additionalProperties",true}};
      gadget.parseInput=[](const Json& value)->Json{
        if(!value.is_object())throw std::invalid_argument("MCP tool input must be an object");
        return value;};
      gadget.inputJSONSchema=value::property(raw,"inputSchema");
      gadget.outputSchema={{"type","string"},{"description","MCP tool execution result"}};
      gadget.description=[description](const Json&,const Json&){return ready(description);};
      gadget.poetry=[description](const gadgets::PoetryContext&){
        const auto text=value::stringLength(description)>2048
          ?value::slice(description,0,2048)+"… [truncated]":description;
        return ready(text);};
      gadget.isConcurrencySafe=[raw](const Json&){return annotation(raw,"readOnlyHint");};
      gadget.isReadOnly=[raw](const Json&){return annotation(raw,"readOnlyHint");};
      gadget.isDestructive=[raw](const Json&){return annotation(raw,"destructiveHint");};
      gadget.isOpenWorld=[raw](const Json&){return annotation(raw,"openWorldHint");};
      gadget.toAutoClassifierInput=[sourceName](const Json& input)->Json{
        return mcpGadgetInputToAutoClassifierInput(input,sourceName);};
      gadget.isSearchOrReadCommand=[services,serverName,sourceName](const Json&){
        return services.classifyForCollapse
          ?services.classifyForCollapse(serverName,sourceName)
          :classifyMcpGadgetForCollapse(serverName,sourceName);};
      gadget.checkPermissions=[fullyQualified](const Json&,const Json&){
        return ready(Json{{"behavior","passthrough"},{"message","MCPTool requires permission."},
          {"suggestions",Json::array({{{"type","addRules"},
            {"rules",Json::array({{{"toolName",fullyQualified}}})},
            {"behavior","allow"},{"destination","localSettings"}}})}});};
      const auto& title=value::property(annotations,"title");
      const auto displayName=title.is_string()&&value::truthy(title)
        ?title.get<std::string>():sourceName;
      gadget.userFacingName=[serverName,displayName](const Json&){
        return serverName+" - "+displayName+" (MCP)";};
      gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& content,const std::string& id){
        return Json{{"tool_use_id",id},{"type","tool_result"},{"content",content}};};
      gadget.isResultTruncated=[services](const Json& output){
        return output.is_string()&&services.isOutputLineTruncated(output.get<std::string>());};
      gadget.renderGadgetUseMessage=services.renderGadgetUseMessage;
      gadget.renderGadgetResultMessage=services.renderGadgetResultMessage;
      gadget.renderGadgetUseProgressMessage=services.renderGadgetUseProgressMessage;
      gadget.call=[connection,services,serverName,sourceName](const Json& args,
          const gadgets::GadgetCallContext& context,
          const std::function<std::future<Json>(const Json&)>&,
          const Json& parentMessage,const std::function<void(const gadgets::GadgetProgress&)>& onProgress){
        return ::batbox::runtime::contextAsync(std::launch::async,[=](){
          const auto id=gadgetUseId(parentMessage);
          if(onProgress&&!id.empty()&&!connection->postToOwner)
            throw std::invalid_argument("Missing MCP owner-strand progress dispatcher");
          const auto emit=[&](const Json& progress){
            if(onProgress&&!id.empty())connection->postToOwner(
              [onProgress,id,progress](){onProgress({id,progress});});};
          if(!id.empty())emit({{"type","mcp_progress"},{"status","started"},
            {"serverName",serverName},{"toolName",sourceName}});
          const auto startTime=services.nowMs();
          for(int attempt=0;;++attempt)try{
            connection->ensureConnected();
            auto elicitation=services.urlElicitation;
            if(!elicitation.invokeOnOwner)elicitation.invokeOnOwner=connection->postToOwner;
            std::function<void(const Json&)> progressCallback;
            if(onProgress&&!id.empty())
              progressCallback=[emit](const Json& progress){emit(progress);};
            const auto raw=callMCPGadgetWithUrlElicitationRetry(*connection,sourceName,args,
              context.stopToken,progressCallback,
              elicitation);
            gadgets::GadgetResult output;
            output.data=raw.aborted?Json():services.processResult(
              callResultAsJson(raw),sourceName,serverName);
            if(!id.empty())emit({{"type","mcp_progress"},{"status","completed"},
              {"serverName",serverName},{"toolName",sourceName},
              {"elapsedTimeMs",services.nowMs()-startTime}});
            if(!raw.meta.is_null()||!raw.structuredContent.is_null()){
              output.mcpMeta=Json::object();
              if(!raw.meta.is_null())output.mcpMeta["_meta"]=raw.meta;
              if(!raw.structuredContent.is_null())
                output.mcpMeta["structuredContent"]=raw.structuredContent;
            }
            return output;
          }catch(const FlightExpiredError&){
            if(attempt<1)continue;
            if(!id.empty())emit({{"type","mcp_progress"},{"status","failed"},
              {"serverName",serverName},{"toolName",sourceName},
              {"elapsedTimeMs",services.nowMs()-startTime}});
            throw;
          }catch(...){
            if(!id.empty())emit({{"type","mcp_progress"},{"status","failed"},
              {"serverName",serverName},{"toolName",sourceName},
              {"elapsedTimeMs",services.nowMs()-startTime}});
            throw;
          }
        });};
      auto built=gadgets::buildGadget(std::move(gadget));
      if((!services.isIncludedGadget||services.isIncludedGadget(built))&&included(built))
        result.push_back(std::move(built));
    }
    return result;
  }catch(const std::exception& error){
    if(services.logError)services.logError(error.what());
    return {};
  }
}
} // namespace batbox::mcp
