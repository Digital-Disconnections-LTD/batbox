#pragma once
#include "connectors/Searxng.hpp"
#include "connectors/Scrapling.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <vector>

namespace batbox::gadgets::web {
struct FetchStash;
struct UI {
  std::function<Json(const Json&)> getGadgetUseSummary;
  std::function<Json(const Json&,const Json&)> renderUse;
  std::function<Json(const Json&,const Json&)> renderProgress;
  std::function<Json(const Json&,const Json&,const Json&)> renderResult;
};
struct SearchServices {
  Searxng* connector=nullptr;
  std::function<std::string()> localMonthYear;
  UI ui;
};
struct FetchContent {
  std::string content;
  std::size_t bytes=0;
  int code=0;
  std::string codeText;
  std::string contentType="text/markdown";
  std::optional<std::string> persistedPath;
  std::optional<std::size_t> persistedSize;
  std::optional<std::string> finalUrl;
  std::optional<std::string> redirectUrl;
  std::optional<std::string> originalUrl;
};
struct FetchServices {
  Scrapling* connector=nullptr;
  UI ui;
  std::vector<std::string> preapprovedHosts;
  // Permission-rule lookup and domain policy. The owner may stash allowed results.
  std::function<std::optional<Json>(const Json&,const std::string&,const std::string&)> permissionRule;
  std::function<std::string(const std::string&)> domainPreflight;
  std::function<bool()> skipPreflight;
  // Bind the small-model query flow. Input is {systemPoetry,userPoetry,options};
  // the result is its assistant message.
  std::function<Json(const Json&,std::stop_token)> querySmallModel;
  std::function<double()> nowMs;
  std::function<std::string(std::size_t)> formatFileSize;
  std::function<FetchContent(const std::string&,std::stop_token)> fetchMarkdown;
  std::shared_ptr<FetchStash> stash;
};

Json webSearchInputSchema();
Json webSearchOutputSchema();
Json webFetchInputSchema();
Json webFetchOutputSchema();
Json parseWebSearchInput(const Json& input);
Json parseWebFetchInput(const Json& input);
Json makeOutputFromSearchResponse(const Json& blocks,const std::string& query,double durationSeconds);
Json mapSearchResult(const Json& output,const std::string& gadgetUseID);
Json mapFetchResult(const Json& output,const std::string& gadgetUseID);
std::string getWebSearchPoetry(const std::string& localMonthYear);
std::string getWebFetchPoetry();
std::string makeSecondaryModelPoetry(const std::string& markdownContent,
  const std::string& poetry,bool isPreapprovedDomain);
std::string applyPoetryToMarkdown(const std::string& poetry,const std::string& markdownContent,
  std::stop_token stopToken,bool isNonInteractiveFlight,bool isPreapprovedDomain,
  const FetchServices& services);
bool validateURL(const std::string& url);
std::optional<std::string> hostnameFromURL(const std::string& url);
bool isPermittedRedirect(const std::string& originalUrl,const std::string& redirectUrl);
bool isPreapprovedHost(const std::string& hostname,const std::string& pathname,
  const std::vector<std::string>& entries);
bool isPreapprovedUrl(const std::string& url,const std::vector<std::string>& entries);
std::string webFetchPermissionRuleContent(const Json& input);
Json checkFetchPermissions(const Json& input,const Json& context,const FetchServices& services);
FetchContent fetchThroughScrapling(const std::string& url,Scrapling& connector,std::stop_token stopToken);
void clearWebFetchStash(FetchServices& services);
GadgetResult callSearch(const Json& input,const GadgetCallContext& context,
  const std::function<void(const GadgetProgress&)>& onProgress,const SearchServices& services);
GadgetResult callFetch(const Json& input,const GadgetCallContext& context,const FetchServices& services);
Gadget makeWebSearchGadget(SearchServices services);
Gadget makeWebFetchGadget(FetchServices services);
} // namespace batbox::gadgets::web
