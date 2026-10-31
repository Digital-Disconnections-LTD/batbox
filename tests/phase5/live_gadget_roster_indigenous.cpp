#include "gadgets/live_roster/LiveRoster.hpp"
#include "mcp/ProcessResult.hpp"
#include "mcp/StdioClient.hpp"
#include "event_loop/Loop.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool value,const char* message){
  if(!value)throw std::runtime_error(message);
}
}
int main(int argc,char** argv){
  check(argc==2,"MCP fixture path required");
  event_loop::Loop owner;
  auto client=std::make_shared<mcp::StdioClient>(
    std::vector<std::string>{"python3",argv[1]},"fixture",
    std::chrono::seconds(3));
  auto connection=mcp::makeStdioConnection(client,"fixture");
  connection->postToOwner=[&](std::function<void()> callback){
    owner.postMission(std::move(callback));};
  mcp::Services mcpServices;
  mcpServices.wire.searchHintMetaKey="test/searchHint";
  mcpServices.wire.alwaysLoadMetaKey="test/alwaysLoad";
  mcpServices.nowMs=[](){return std::int64_t{1000};};
  mcpServices.isOutputLineTruncated=[](const std::string&){return false;};
  mcpServices.renderGadgetUseMessage=[](const Json&,const Json&){return Json();};
  mcpServices.renderGadgetResultMessage=[](const Json&,const Json&,
    const Json&){return Json();};
  mcpServices.renderGadgetUseProgressMessage=[](const Json&,const Json&){
    return Json();};
  mcp::ProcessResultServices processor;
  processor.result.nowMs=[](){return std::int64_t{1000};};
  processor.result.randomSuffix=[](){return std::string("fixture");};
  processor.validation.configuredMaxOutputTokens=25000;
  mcp::bindIndigenousProcessResult(mcpServices,processor);
  auto discovered=mcp::fetchGadgetsForClient(connection,mcpServices);
  check(discovered.size()==3,"three real stdio tools discovered");

  auto old=std::make_shared<gadgets::live_roster::Snapshot>();
  old->mcpConnections.push_back(connection);
  old->searxng=std::make_shared<Searxng>("http://127.0.0.1:8888/search");
  gadgets::web::SearchServices search;
  search.connector=old->searxng.get();
  search.localMonthYear=[](){return std::string("October 2026");};
  search.ui.getGadgetUseSummary=[](const Json&){return Json("query");};
  search.ui.renderUse=[](const Json&,const Json&){return Json();};
  search.ui.renderProgress=[](const Json&,const Json&){return Json();};
  search.ui.renderResult=[](const Json&,const Json&,const Json&){return Json();};
  old->owned.push_back(gadgets::web::makeWebSearchGadget(search));
  old->base.push_back(&old->owned.back());
  for(auto& gadget:discovered){
    old->owned.push_back(std::move(gadget));
    old->mcp.push_back(&old->owned.back());
  }
  gadgets::CatalogSettings settings;
  settings.getDenyRuleForGadget=[](const gadgets::Gadget& gadget,const Json& rules){
    const auto& denied=value::property(rules,"deny");
    if(denied.is_array())for(const auto& name:denied)
      if(name==gadget.name)return true;
    return false;};
  settings.localeCompare=[](const std::string& left,const std::string& right){
    return left.compare(right);};
  gadgets::live_roster::Roster roster;
  roster.publish(old);
  auto selection=roster.selectCurrent(Json::object(),settings);
  check(selection.gadgets.size()==4,"web and MCP tools selected");
  check(selection.gadgets.front()->name=="WebSearch","base tool first");
  auto denied=roster.selectCurrent(Json{{"deny",Json::array(
    {"mcp__fixture__slow"})}},settings);
  check(denied.gadgets.size()==3,"permission deny filter");
  check(selection.owner->find("mcp__fixture__echo"),"MCP lookup");
  roster.publish(std::make_shared<gadgets::live_roster::Snapshot>());
  check(roster.current()->owned.empty(),"registry refreshed");
  check(roster.retiredCount()==1,"old snapshot retained for raw pointers");
  const auto* echo=selection.owner->find("mcp__fixture__echo");
  const auto parent=Json{{"message",Json{{"content",Json::array(
    {Json{{"type","tool_use"},{"id","registry"}}})}}}};
  const auto permission=[](const Json&){
    std::promise<Json> promise;promise.set_value(Json());
    return promise.get_future();};
  auto outcome=echo->call(Json{{"text","hello"}},
    gadgets::GadgetCallContext{},permission,parent,{}).get();
  owner.runReady();
  check(outcome.data=="{\"echo\":\"hello\"}",
    "old snapshot retains callable MCP connection");
  roster.releaseRetiredAtQuiescence();
  check(roster.retiredCount()==0,"quiescent release");
  std::cout<<"live tool registry selection and MCP lifetime passed\n";
}
