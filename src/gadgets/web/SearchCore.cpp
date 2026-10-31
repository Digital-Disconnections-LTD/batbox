#include "gadgets/web/Web.hpp"
#include "connectors/Abort.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
namespace {std::string textValue(const Json& value){
  return value.is_string()?value.get<std::string>():value.is_null()?"undefined":value::stringify(value);
}}
Json makeOutputFromSearchResponse(const Json& blocks,const std::string& query,double durationSeconds){
  Json results=Json::array();
  std::string textAcc;
  bool inText=true;
  for(const auto& block:blocks){
    const std::string type=block.value("type",std::string());
    if(type=="server_tool_use"){
      if(inText){
        inText=false;
        const auto trimmed=value::trim(textAcc);
        if(!trimmed.empty())results.push_back(trimmed);
        textAcc.clear();
      }
      continue;
    }
    if(type=="web_search_tool_result"){
      const auto content=value::property(block,"content");
      if(!content.is_array()){
        results.push_back("Web search error: "+textValue(value::property(content,"error_code")));
        continue;
      }
      Json hits=Json::array();
      for(const auto& row:content)hits.push_back({{"title",value::property(row,"title")},
        {"url",value::property(row,"url")}});
      results.push_back({{"tool_use_id",value::property(block,"tool_use_id")},{"content",hits}});
    }
    if(type=="text"){
      const auto text=textValue(value::property(block,"text"));
      if(inText)textAcc+=text;
      else {inText=true;textAcc=text;}
    }
  }
  if(!textAcc.empty())results.push_back(value::trim(textAcc));
  return {{"query",query},{"results",results},{"durationSeconds",durationSeconds}};
}

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
