#include "mcp/Validation.hpp"
#include "mcp/Codec.hpp"
#include "media/Media.hpp"
#include "services/tokenEstimation.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::mcp {
namespace {
constexpr double imageTokenEstimate=1600;
constexpr double thresholdFactor=0.5;
constexpr double defaultMaxTokens=25000;
std::string truncationMessage(double maxTokens){
  return "\n\n[OUTPUT TRUNCATED - exceeded "+value::numberToString(maxTokens)+" token limit]\n\n"
    "The tool output was truncated. If this MCP server provides pagination or filtering tools, "
    "use them to retrieve specific portions of the data. If pagination is not available, "
    "inform the user that you are working with truncated output and results may be incomplete.";
}
Json compressImageBlock(const Json& block,std::size_t maxBytes){
  const auto& source=value::property(block,"source");
  if(value::property(source,"type")!="base64")return block;
  const auto& data=value::property(source,"data");
  if(!data.is_string())return block;
  const auto bytes=decodeBase64(data.get<std::string>());
  if(bytes.size()<=maxBytes)return block;
  const auto compressed=media::compressImageBuffer(bytes,maxBytes);
  return {{"type","image"},{"source",{{"type","base64"},
    {"media_type",compressed.mediaType},{"data",compressed.base64}}}};
}
Json truncateContentBlocks(const Json& blocks,double maxChars,const ValidationServices& services){
  Json result=Json::array();double currentChars=0;
  for(const auto& block:blocks){
    const auto& type=value::property(block,"type");
    if(type=="text"){
      const double remaining=maxChars-currentChars;
      if(remaining<=0)break;
      const auto& text=value::property(block,"text");
      const auto content=text.is_string()?text.get<std::string>():"";
      const auto length=value::stringLength(content);
      if(length<=remaining){result.push_back(block);currentChars+=length;}
      else{result.push_back({{"type","text"},{"text",value::slice(content,0,remaining)}});break;}
    }else if(type=="image"){
      const double imageChars=imageTokenEstimate*4;
      if(currentChars+imageChars<=maxChars){result.push_back(block);currentChars+=imageChars;}
      else{
        const double remaining=maxChars-currentChars;
        if(remaining>0)try{
          const auto bytes=static_cast<std::size_t>(std::floor(remaining*0.75));
          const auto compressed=services.compressImageBlock
            ?services.compressImageBlock(block,bytes):compressImageBlock(block,bytes);
          result.push_back(compressed);
          const auto& source=value::property(compressed,"source");
          const auto& data=value::property(source,"data");
          currentChars+=value::property(source,"type")=="base64"&&data.is_string()
            ?value::stringLength(data.get<std::string>()):imageChars;
        }catch(const std::exception&){}
      }
    }else result.push_back(block);
  }
  return result;
}
}
double getMaxMcpOutputTokens(const ValidationServices& services){
  if(services.envMaxOutputTokens&&!services.envMaxOutputTokens->empty()){
    const double parsed=value::parseInt10(*services.envMaxOutputTokens);
    if(std::isfinite(parsed)&&parsed>0)return parsed;
  }
  if(services.configuredMaxOutputTokens&&std::isfinite(*services.configuredMaxOutputTokens)&&
     *services.configuredMaxOutputTokens>0)return *services.configuredMaxOutputTokens;
  return defaultMaxTokens;
}
double getContentSizeEstimate(const Json& content){
  if(!value::truthy(content))return 0;
  if(content.is_string())return roughTokenCountEstimation(content.get<std::string>());
  if(!content.is_array())return 0;
  double total=0;
  for(const auto& block:content){
    const auto& type=value::property(block,"type");
    if(type=="text"){
      const auto& text=value::property(block,"text");
      if(text.is_string())total+=roughTokenCountEstimation(text.get<std::string>());
    }else if(type=="image")total+=imageTokenEstimate;
  }
  return total;
}
bool mcpContentNeedsTruncation(const Json& content,const ValidationServices& services){
  if(!value::truthy(content))return false;
  const auto maxTokens=getMaxMcpOutputTokens(services);
  if(getContentSizeEstimate(content)<=maxTokens*thresholdFactor)return false;
  if(!services.countMessagesTokensWithAPI)
    throw std::invalid_argument("Missing MCP token-count import");
  try{
    const auto count=services.countMessagesTokensWithAPI(
      Json::array({{{"role","user"},{"content",content}}}),Json::array());
    return count&&*count>maxTokens;
  }catch(const std::exception& error){
    if(services.logError)services.logError(error.what());
    return false;
  }
}
Json truncateMcpContent(const Json& content,const ValidationServices& services){
  if(!value::truthy(content))return content;
  const auto maxTokens=getMaxMcpOutputTokens(services);
  const double maxChars=maxTokens*4;
  const auto message=truncationMessage(maxTokens);
  if(content.is_string())return value::slice(content.get<std::string>(),0,maxChars)+message;
  if(content.is_array()){
    auto blocks=truncateContentBlocks(content,maxChars,services);
    blocks.push_back({{"type","text"},{"text",message}});
    return blocks;
  }
  return content;
}
Json truncateMcpContentIfNeeded(const Json& content,const ValidationServices& services){
  return mcpContentNeedsTruncation(content,services)
    ?truncateMcpContent(content,services):content;
}
} // namespace batbox::mcp
