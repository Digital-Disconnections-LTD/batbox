#include "gadgets/web/Web.hpp"
#include "connectors/Abort.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
std::string applyPoetryToMarkdown(const std::string& poetry,const std::string& markdownContent,
  std::stop_token stopToken,bool isNonInteractiveFlight,bool isPreapprovedDomain,
  const FetchServices& services){
  if(!services.querySmallModel)throw std::invalid_argument("Missing WebFetch querySmallModel binding");
  const auto truncated=value::stringLength(markdownContent)>100000
    ?value::slice(markdownContent,0,100000)+"\n\n[Content truncated due to length...]"
    :markdownContent;
  const auto modelPoetry=makeSecondaryModelPoetry(truncated,poetry,isPreapprovedDomain);
  const Json request={{"systemPrompt",Json::array()},
    {"userPrompt",modelPoetry},
    {"options",{{"querySource","web_fetch_apply"},{"agents",Json::array()},
      {"isNonInteractiveSession",isNonInteractiveFlight},
      {"hasAppendSystemPrompt",false},{"mcpTools",Json::array()}}}};
  const auto answer=services.querySmallModel(request,stopToken);
  checkConnectorAbort(stopToken);
  const auto message=value::property(answer,"message");
  const auto content=value::property(message,"content");
  if(content.is_array()&&!content.empty()){
    const auto text=value::property(content.front(),"text");
    if(!text.is_null()&&text.is_string())return text.get<std::string>();
  }
  return "No response from model";
}
} // namespace batbox::gadgets::web
