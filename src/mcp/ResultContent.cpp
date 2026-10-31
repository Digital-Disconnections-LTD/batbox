#include "mcp/Mcp.hpp"
#include "mcp/Codec.hpp"
#include "mcp/OutputStorage.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::mcp {
namespace {
std::string mimeExtension(const Json& item){
  const auto& mime=value::property(item,"mimeType");
  if(!mime.is_string())return "png";
  const auto text=mime.get<std::string>();
  const auto slash=text.find('/');
  if(slash==std::string::npos)return "png";
  const auto next=text.find('/',slash+1);
  const auto ext=text.substr(slash+1,next==std::string::npos?next:next-slash-1);
  return ext.empty()?"png":ext;
}
Json imageBlock(const std::string& bytes,const std::string& ext,const ResultServices& services){
  if(!services.resizeImage)throw std::invalid_argument("Missing MCP image resize import");
  const auto resized=services.resizeImage(bytes,bytes.size(),ext);
  return {{"type","image"},{"source",{{"data",encodeBase64(resized.bytes)},
    {"media_type","image/"+resized.mediaType},{"type","base64"}}}};
}
Json persistBlobToTextBlock(const std::string& bytes,const std::optional<std::string>& mimeType,
  const std::string& serverName,const std::string& sourceDescription,
  const ResultServices& services){
  if(!services.persistBinaryContent||!services.nowMs||!services.randomSuffix)
    throw std::invalid_argument("Missing MCP binary storage, clock or random import");
  const auto persistId="mcp-"+normalizeNameForMCP(serverName,services.hostedServerPrefix)+
    "-blob-"+std::to_string(services.nowMs())+"-"+services.randomSuffix();
  const auto persisted=services.persistBinaryContent(bytes,mimeType,persistId);
  if(persisted.contains("error"))return Json::array({{{"type","text"},
    {"text",sourceDescription+"Binary content ("+mimeType.value_or("unknown type")+
      ", "+std::to_string(bytes.size())+" bytes) could not be saved to disk: "+
      persisted.at("error").get<std::string>()}}});
  const auto size=persisted.at("size").get<double>();
  return Json::array({{{"type","text"},{"text",getBinaryBlobSavedMessage(
    persisted.at("filepath").get<std::string>(),mimeType,size,sourceDescription)}}});
}
}
Json transformResultContent(const Json& item,const std::string& serverName,
  const ResultServices& services){
  const auto& type=value::property(item,"type");
  if(type=="text")return Json::array({{{"type","text"},{"text",value::property(item,"text")}}});
  if(type=="audio"){
    const auto data=jsStringValue(value::property(item,"data"));
    const auto& mime=value::property(item,"mimeType");
    return persistBlobToTextBlock(decodeBase64(data),mime.is_string()
      ?std::optional<std::string>(mime.get<std::string>()):std::nullopt,
      serverName,"[Audio from "+serverName+"] ",services);
  }
  if(type=="image"){
    const auto bytes=decodeBase64(jsStringValue(value::property(item,"data")));
    return Json::array({imageBlock(bytes,mimeExtension(item),services)});
  }
  if(type=="resource"){
    const auto& resource=value::property(item,"resource");
    const auto prefix="[Resource from "+serverName+" at "+
      jsStringValue(value::property(resource,"uri"))+"] ";
    if(resource.is_object()&&resource.contains("text"))
      return Json::array({{{"type","text"},{"text",prefix+
        jsStringValue(resource.at("text"))}}});
    if(resource.is_object()&&resource.contains("blob")){
      const auto& mime=value::property(resource,"mimeType");
      const auto mimeType=mime.is_string()
        ?std::optional<std::string>(mime.get<std::string>()):std::nullopt;
      const auto bytes=decodeBase64(jsStringValue(resource.at("blob")));
      const bool isImage=mimeType&&(*mimeType=="image/jpeg"||*mimeType=="image/png"||
        *mimeType=="image/gif"||*mimeType=="image/webp");
      if(isImage)return Json::array({{{"type","text"},{"text",prefix}},
        imageBlock(bytes,mimeExtension(resource),services)});
      return persistBlobToTextBlock(bytes,mimeType,serverName,prefix,services);
    }
    return Json::array();
  }
  if(type=="resource_link"){
    std::string text="[Resource link: "+jsStringValue(value::property(item,"name"))+"] "+
      jsStringValue(value::property(item,"uri"));
    const auto& description=value::property(item,"description");
    if(value::truthy(description))text+=" ("+jsStringValue(description)+")";
    return Json::array({{{"type","text"},{"text",text}}});
  }
  return Json::array();
}

TransformedResult transformMCPResult(const Json& result,const std::string& gadget,
  const std::string& serverName,const ResultServices& services){
  if(result.is_object()){
    if(result.contains("toolResult"))return {jsStringValue(result.at("toolResult")),"toolResult",""};
    if(result.contains("structuredContent")){
      const auto& structured=result.at("structuredContent");
      return {value::stringify(structured),"structuredContent",inferCompactSchema(structured)};
    }
    if(result.contains("content")&&result.at("content").is_array()){
      Json blocks=Json::array();
      for(const auto& item:result.at("content"))
        for(const auto& block:transformResultContent(item,serverName,services))blocks.push_back(block);
      return {blocks,"contentArray",inferCompactSchema(blocks)};
    }
  }
  throw std::runtime_error("MCP server \""+serverName+"\" tool \""+gadget+
    "\": unexpected response format");
}
} // namespace batbox::mcp
