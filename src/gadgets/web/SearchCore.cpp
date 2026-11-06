#include "gadgets/web/Web.hpp"
#include "connectors/Abort.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
namespace {std::string textValue(const Json& value){
  return value.is_string()?value.get<std::string>():value.is_null()?"undefined":value::stringify(value);
}}
Json mapSearchResult(const Json& output,const std::string& gadgetUseID){
  const auto query=textValue(value::property(output,"query"));
  std::string formatted="Web search results for query: \""+query+"\"\n\n";
  const auto results=value::property(output,"results");
  if(results.is_array())for(const auto& result:results){
    if(result.is_null())continue;
    if(result.is_string())formatted+=result.get<std::string>()+"\n\n";
    else {
      const auto content=value::property(result,"content");
      if(content.is_array()&&!content.empty())formatted+="Links: "+value::stringify(content)+"\n\n";
      else formatted+="No links found.\n\n";
    }
  }
  formatted+="\nREMINDER: You MUST include the sources above in your response to the user using markdown hyperlinks.";
  return {{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",value::trim(formatted)}};
}

GadgetResult callSearch(const Json& input,const GadgetCallContext& context,
  const std::function<void(const GadgetProgress&)>& onProgress,const SearchServices& services){
  if(!services.connector)throw std::invalid_argument("Missing WebSearch connector: SearXNG");
  checkConnectorAbort(context.stopToken);
  auto output=services.connector->search(input,"search-progress-1",std::chrono::minutes(2),context.stopToken);
  checkConnectorAbort(context.stopToken);
  if(onProgress){
    const auto results=value::property(output,"results");
    std::size_t count=0;
    if(results.is_array())for(const auto& result:results){
      const auto hits=value::property(result,"content");
      if(hits.is_array())count+=hits.size();
    }
    onProgress({"search-progress-1",{{"type","search_results_received"},
      {"resultCount",count},{"query",input.at("query")}}});
  }
  GadgetResult result;result.data=std::move(output);return result;
}
} // namespace batbox::gadgets::web
