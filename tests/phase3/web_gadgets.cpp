#include "gadgets/web/Web.hpp"
#include <cassert>
#include <iostream>

using batbox::Json;
namespace web=batbox::gadgets::web;
static web::UI ui(){
  web::UI result;
  result.getGadgetUseSummary=[](const Json& input)->Json{
    if(input.contains("query"))return input.at("query");
    if(input.contains("url"))return input.at("url");return Json();};
  result.renderUse=[](const Json&,const Json&)->Json{return Json();};
  result.renderProgress=[](const Json&,const Json&)->Json{return Json();};
  result.renderResult=[](const Json&,const Json&,const Json&)->Json{return Json();};
  return result;
}
int main(int argc,char** argv){
  assert(argc==3);
  batbox::Searxng searxng(argv[1]);
  web::SearchServices search;search.connector=&searxng;search.localMonthYear=[](){return std::string("October 2026");};search.ui=ui();
  auto searchGadget=web::makeWebSearchGadget(search);
  assert(searchGadget.inputJSONSchema["required"].size()==1);
  assert(searchGadget.shouldDefer.value()&&searchGadget.isReadOnly(Json()));
  assert(searchGadget.parseInput(Json{{"query","two words"}})["query"]=="two words");
  assert(!searchGadget.validateInput(Json{{"query","ok"},{"allowed_domains",Json::array({"a"})},
    {"blocked_domains",Json::array({"b"})}},Json()).get().result);
  auto permission=searchGadget.checkPermissions(Json(),Json()).get();
  assert(permission["behavior"]=="passthrough");
  std::vector<batbox::gadgets::GadgetProgress> progress;
  Json query={{"query","two words"},{"allowed_domains",Json::array({"example.org"})}};
  auto found=searchGadget.call(query,{}, {},Json(),[&](const auto& p){progress.push_back(p);}).get();
  assert(found.data["results"][0]["content"].size()==1);
  assert(progress.size()==1&&progress[0].data["resultCount"]==1);
  assert(searchGadget.mapGadgetResultToGadgetResultBlockParam(found.data,"u1")["content"].get<std::string>().find("Links:")!=std::string::npos);

  batbox::Scrapling scrapling({argv[2]},std::chrono::seconds(5));
  int preflights=0,modelCalls=0;double clock=100;
  web::FetchServices fetch;fetch.connector=&scrapling;fetch.ui=ui();
  fetch.preapprovedHosts={"docs.example.org"};
  fetch.permissionRule=[](const Json&,const std::string& mode,const std::string& content)->std::optional<Json>{
    if(mode=="deny"&&content=="domain:denied.example.org")return Json{{"rule","deny"}};
    return std::nullopt;};
  fetch.skipPreflight=[](){return false;};
  fetch.domainPreflight=[&](const std::string&){++preflights;return std::string("allowed");};
  fetch.nowMs=[&](){return ++clock;};
  fetch.querySmallModel=[&](const Json& request,std::stop_token){
    ++modelCalls;
    assert(request["options"]["querySource"]=="web_fetch_apply");
    assert(request["userPrompt"].get<std::string>().find("# Article")!=std::string::npos);
    return Json{{"type","assistant"},{"message",{{"content",Json::array({{{"type","text"},{"text","model answer"}}})}}}};
  };
  auto fetchGadget=web::makeWebFetchGadget(fetch);
  assert(fetchGadget.inputJSONSchema["required"].size()==2);
  assert(fetchGadget.shouldDefer.value()&&fetchGadget.isReadOnly(Json()));
  const Json docs={{"url","https://docs.example.org/a"},{"prompt","Summarize"}};
  assert(fetchGadget.checkPermissions(docs,Json()).get()["behavior"]=="allow");
  auto direct=fetchGadget.call(docs,{}, {},Json(),{}).get();
  assert(direct.data["result"]=="# Documentation"&&modelCalls==0);
  auto directStashed=fetchGadget.call(docs,{}, {},Json(),{}).get();
  assert(directStashed.data["result"]=="# Documentation"&&preflights==1);
  const Json article={{"url","https://ordinary.example.org/a"},{"prompt","Answer"}};
  assert(fetchGadget.checkPermissions(article,Json()).get()["behavior"]=="ask");
  auto processed=fetchGadget.call(article,{}, {},Json(),{}).get();
  assert(processed.data["result"]=="model answer"&&modelCalls==1);
  assert(fetchGadget.mapGadgetResultToGadgetResultBlockParam(processed.data,"u2")["content"]=="model answer");
  const Json denied={{"url","https://denied.example.org/a"},{"prompt","x"}};
  assert(fetchGadget.checkPermissions(denied,Json()).get()["behavior"]=="deny");
  const Json redirect={{"url","https://redirect.example.org/a"},{"prompt","x"}};
  auto moved=fetchGadget.call(redirect,{}, {},Json(),{}).get();
  assert(moved.data["url"]=="https://else.example.org/final"&&moved.data["code"]==200);
  assert(moved.data["result"]=="model answer"&&modelCalls==2);
  std::cout<<"web tool factories, real connector calls, permissions, cache and model binding passed\n";
}
