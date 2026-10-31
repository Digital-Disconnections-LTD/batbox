#include "normalization/Attachments.hpp"
#include "normalization/Internal.hpp"
#include <vector>

namespace batbox::normalization {
using namespace internal;
namespace {
std::string str(const Json& value){if(value.is_string())return value.get<std::string>();if(value.is_null())return "null";return value::stringify(value);}
std::string val(const Json& object,const char* key){return str(get(object,key));}
std::string join(const std::vector<std::string>& parts,const std::string& separator){
  std::string result;bool first=true;for(const auto& part:parts){if(!first)result+=separator;result+=part;first=false;}return result;
}
std::vector<std::string> strings(const Json& array){std::vector<std::string> result;for(const auto& item:array)result.push_back(str(item));return result;}
Json meta(const Json& content,const AttachmentServices& services){
  need(services.messageServices!=nullptr,"AttachmentServices.messageServices");
  return messages::createUserMessage(Json{{"content",content},{"isMeta",true}},*services.messageServices);
}
Json one(const Json& content,const AttachmentServices& services){return wrapMessagesInSystemReminder(Json::array({meta(content,services)}));}
} // namespace

std::optional<Json> normalizeDeltaAttachmentForAPI(const Json& attachment,const AttachmentServices& services){
  const std::string type=val(attachment,"type");
  if(type=="deferred_tools_delta"){
    std::vector<std::string> parts;
    const Json& added=get(attachment,"addedLines"),&removed=get(attachment,"removedNames");
    if(!added.empty())parts.push_back("The following deferred tools are now available via ToolSearch:\n"+join(strings(added),"\n"));
    if(!removed.empty())parts.push_back("The following deferred tools are no longer available (their MCP server disconnected). Do not search for them — ToolSearch will return no match:\n"+join(strings(removed),"\n"));
    return one(join(parts,"\n\n"),services);
  }
  if(type=="agent_listing_delta"){
    std::vector<std::string> parts;
    const Json& added=get(attachment,"addedLines"),&removed=get(attachment,"removedTypes");
    if(!added.empty()){
      const std::string header=value::truthy(get(attachment,"isInitial"))?"Available agent types for the Agent tool:":"New agent types are now available for the Agent tool:";
      parts.push_back(header+"\n"+join(strings(added),"\n"));
    }
    if(!removed.empty()){
      std::vector<std::string> names;for(const auto& item:removed)names.push_back("- "+str(item));
      parts.push_back("The following agent types are no longer available:\n"+join(names,"\n"));
    }
    if(value::truthy(get(attachment,"isInitial")) && value::truthy(get(attachment,"showConcurrencyNote")))
      parts.push_back("Launch multiple agents concurrently whenever possible, to maximize performance; to do that, use a single message with multiple tool uses.");
    return one(join(parts,"\n\n"),services);
  }
  if(type=="mcp_instructions_delta"){
    std::vector<std::string> parts;
    const Json& added=get(attachment,"addedBlocks"),&removed=get(attachment,"removedNames");
    if(!added.empty())parts.push_back("# MCP Server Instructions\n\nThe following MCP servers have provided instructions for how to use their tools and resources:\n\n"+join(strings(added),"\n\n"));
    if(!removed.empty())parts.push_back("The following MCP servers have disconnected. Their instructions above no longer apply:\n"+join(strings(removed),"\n"));
    return one(join(parts,"\n\n"),services);
  }
  if(type=="nested_memory")return one("Contents of "+val(get(attachment,"content"),"path")+":\n\n"+val(get(attachment,"content"),"content"),services);
  if(type=="relevant_memories"){
    need(static_cast<bool>(services.memoryHeader),"AttachmentServices.memoryHeader");
    Json messages=Json::array();
    for(const auto& memory:get(attachment,"memories")){
      const std::string header=!get(memory,"header").is_null()?val(memory,"header"):
        services.memoryHeader(val(memory,"path"),get(memory,"mtimeMs"));
      messages.push_back(meta(header+"\n\n"+val(memory,"content"),services));
    }
    return wrapMessagesInSystemReminder(messages);
  }
  if(type=="selected_lines_in_ide"){
    std::string content=val(attachment,"content");
    if(value::stringLength(content)>2000){content=value::substring(content,0,2000)+"\n... (truncated)";}
    return one("The user selected the lines "+val(attachment,"lineStart")+" to "+val(attachment,"lineEnd")+" from "+val(attachment,"filename")+":\n"+content+"\n\nThis may or may not be related to the current task.",services);
  }
  if(type=="opened_file_in_ide")return one("The user opened the file "+val(attachment,"filename")+" in the IDE. This may or may not be related to the current task.",services);
  if(type=="plan_file_reference")return one("A plan file exists from plan mode at: "+val(attachment,"planFilePath")+"\n\nPlan contents:\n\n"+val(attachment,"planContent")+"\n\nIf this plan is relevant to the current work and not already complete, continue working on it.",services);
  if(type=="compact_file_reference")return one("Note: "+val(attachment,"filename")+" was read before the last conversation was summarized, but the contents are too large to include. Use "+services.fileReadGadgetName+" tool if you need to access it.",services);
  if(type=="pdf_reference"){
    need(static_cast<bool>(services.formatFileSize),"AttachmentServices.formatFileSize");
    const std::string name=services.fileReadGadgetName;
    return one("PDF file: "+val(attachment,"filename")+" ("+val(attachment,"pageCount")+" pages, "+services.formatFileSize(get(attachment,"fileSize").get<double>())+"). "
      "This PDF is too large to read all at once. You MUST use the "+name+" tool with the pages parameter "
      "to read specific page ranges (e.g., pages: \"1-5\"). Do NOT call "+name+" without the pages parameter "
      "or it will fail. Start by reading the first few pages to understand the structure, then read more as needed. "
      "Maximum 20 pages per request.",services);
  }
  if(type=="mcp_resource"){
    const Json& content=get(attachment,"content");
    const Json& items=get(content,"contents");
    if(!value::truthy(content)||!items.is_array()||items.empty())
      return one("<mcp-resource server=\""+val(attachment,"server")+"\" uri=\""+val(attachment,"uri")+"\">(No content)</mcp-resource>",services);
    Json blocks=Json::array();
    for(const auto& item:items){
      if(!item.is_object())continue;
      if(get(item,"text").is_string()){
        blocks.push_back(textBlock("Full contents of resource:"));
        blocks.push_back(textBlock(val(item,"text")));
        blocks.push_back(textBlock("Do NOT read this resource again unless you think it may have changed, since you already have the full contents."));
      }else if(has(item,"blob")){
        const std::string mime=has(item,"mimeType")?val(item,"mimeType"):"application/octet-stream";
        blocks.push_back(textBlock("[Binary content: "+mime+"]"));
      }
    }
    if(!blocks.empty())return one(blocks,services);
    return one("<mcp-resource server=\""+val(attachment,"server")+"\" uri=\""+val(attachment,"uri")+"\">(No displayable content)</mcp-resource>",services);
  }
  return std::nullopt;
}

} // namespace batbox::normalization
