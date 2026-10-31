#include "runtime/AsyncLaunch.hpp"
#include "gadgets/web/Web.hpp"
#include "gadgets/web/FetchStash.hpp"
#include <future>
#include <memory>
#include <stdexcept>

namespace batbox::gadgets::web {
namespace {
void requireUI(const UI& ui){
  if(!ui.getGadgetUseSummary||!ui.renderUse||!ui.renderProgress||!ui.renderResult)
    throw std::invalid_argument("Missing web tool UI import");
}
std::future<std::string> ready(std::string value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
std::future<ValidationResult> ready(ValidationResult value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
std::future<Json> ready(Json value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
Json activity(const Json& input,const UI& ui,const std::string& action,const std::string& fallback){
  const auto summary=ui.getGadgetUseSummary(input);
  return value::truthy(summary)?Json(action+summary.get<std::string>()):Json(fallback);
}
}
Gadget makeWebSearchGadget(SearchServices services){
  if(!services.connector)throw std::invalid_argument("Missing WebSearch connector: SearXNG");
  requireUI(services.ui);
  if(!services.localMonthYear)throw std::invalid_argument("Missing localMonthYear binding");
  auto state=std::make_shared<SearchServices>(std::move(services));
  Gadget gadget;
  gadget.name="WebSearch";gadget.searchHint="search the web for current information";
  gadget.maxResultSizeChars=100000;gadget.shouldDefer=true;
  gadget.inputJSONSchema=webSearchInputSchema();gadget.outputSchema=webSearchOutputSchema();
  gadget.parseInput=parseWebSearchInput;
  gadget.description=[](const Json& input,const Json&){
    return ready("Batbox wants to search the web for: "+input.value("query",std::string()));};
  gadget.poetry=[state](const PoetryContext&){return ready(getWebSearchPoetry(state->localMonthYear()));};
  gadget.userFacingName=[](const Json&){return std::string("Web Search");};
  gadget.getGadgetUseSummary=state->ui.getGadgetUseSummary;
  gadget.getActivityDescription=[state](const Json& input){
    return activity(input,state->ui,"Searching for ","Searching the web");};
  gadget.isEnabled=[state]{return state->connector!=nullptr;};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return value::property(input,"query");};
  gadget.checkPermissions=[](const Json&,const Json&){return ready(Json{
    {"behavior","passthrough"},{"message","WebSearch requires permission."},
    {"suggestions",Json::array({{{"type","addRules"},
      {"rules",Json::array({{{"toolName","WebSearch"}}})},
      {"behavior","allow"},{"destination","localSettings"}}})}});};
  gadget.renderGadgetUseMessage=state->ui.renderUse;
  gadget.renderGadgetUseProgressMessage=state->ui.renderProgress;
  gadget.renderGadgetResultMessage=state->ui.renderResult;
  gadget.extractSearchText=[](const Json&){return std::string();};
  gadget.validateInput=[](const Json& input,const Json&){
    const auto query=input.value("query",std::string());
    if(query.empty())return ready(ValidationResult{false,"Error: Missing query",1});
    const auto allowed=value::property(input,"allowed_domains"),blocked=value::property(input,"blocked_domains");
    if(allowed.is_array()&&!allowed.empty()&&blocked.is_array()&&!blocked.empty())
      return ready(ValidationResult{false,
        "Error: Cannot specify both allowed_domains and blocked_domains in the same request",2});
    return ready(ValidationResult{});
  };
  gadget.call=[state](const Json& input,const GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>& progress){
    return ::batbox::runtime::contextAsync(std::launch::deferred,[state,input,context,progress]{
      return callSearch(input,context,progress,*state);});
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=mapSearchResult;
  return buildGadget(std::move(gadget));
}

Gadget makeWebFetchGadget(FetchServices services){
  if(!services.connector)throw std::invalid_argument("Missing WebFetch connector: Scrapling");
  requireUI(services.ui);
  if(!services.permissionRule||!services.skipPreflight||!services.nowMs)
    throw std::invalid_argument("Missing WebFetch permissions/config/time binding");
  if(!services.stash)services.stash=std::make_shared<FetchStash>();
  auto state=std::make_shared<FetchServices>(std::move(services));
  Gadget gadget;
  gadget.name="WebFetch";gadget.searchHint="fetch and extract content from a URL";
  gadget.maxResultSizeChars=100000;gadget.shouldDefer=true;
  gadget.inputJSONSchema=webFetchInputSchema();gadget.outputSchema=webFetchOutputSchema();
  gadget.parseInput=parseWebFetchInput;
  gadget.description=[](const Json& input,const Json&){
    const auto host=hostnameFromURL(input.value("url",std::string()));
    return ready(host?"Batbox wants to fetch content from "+*host:
      "Batbox wants to fetch content from this URL");};
  gadget.poetry=[](const PoetryContext&){return ready(getWebFetchPoetry());};
  gadget.userFacingName=[](const Json&){return std::string("Fetch");};
  gadget.getGadgetUseSummary=state->ui.getGadgetUseSummary;
  gadget.getActivityDescription=[state](const Json& input){
    return activity(input,state->ui,"Fetching ","Fetching web page");};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    const auto url=input.value("url",std::string()),poetry=input.value("prompt",std::string());
    return poetry.empty()?url:url+": "+poetry;};
  gadget.checkPermissions=[state](const Json& input,const Json& context){
    return ready(checkFetchPermissions(input,context,*state));};
  gadget.renderGadgetUseMessage=state->ui.renderUse;
  gadget.renderGadgetUseProgressMessage=state->ui.renderProgress;
  gadget.renderGadgetResultMessage=state->ui.renderResult;
  gadget.validateInput=[](const Json& input,const Json&){
    const auto url=input.value("url",std::string());
    if(!hostnameFromURL(url))return ready(ValidationResult{false,
      "Error: Invalid URL \""+url+"\". The URL provided could not be parsed.",1,
      std::nullopt,Json{{"reason","invalid_url"}}});
    return ready(ValidationResult{});
  };
  gadget.call=[state](const Json& input,const GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){
    return ::batbox::runtime::contextAsync(std::launch::deferred,[state,input,context]{return callFetch(input,context,*state);});};
  gadget.mapGadgetResultToGadgetResultBlockParam=mapFetchResult;
  return buildGadget(std::move(gadget));
}
} // namespace batbox::gadgets::web
