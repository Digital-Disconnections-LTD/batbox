#include "query/OwnerModelCursor.hpp"
#include "query/OwnerSegmentInvoker.hpp"
#include <cassert>
#include <future>
#include <thread>
#include <vector>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr failure;
  pending.then([&](const T* item,const std::exception_ptr& error){
    if(item)value=*item;failure=error;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::logic_error("Owner model cursor did not settle");
  return *value;
}
}
int main(){
  event_loop::Loop loop;
  const auto dispatcher=loop.dispatcher();
  std::vector<std::string> order;
  int atomicResult=0;
  std::thread worker([&]{
    atomicResult=query::invokeOwnerSegment(dispatcher,std::stop_token{},[&]{
      assert(loop.isOwnerThread());
      order.push_back("first");
      loop.queueMicrotask([&]{order.push_back("microtask");});
      order.push_back("second");
      return 17;
    });
    dispatcher.postMission([&]{loop.stop();});
  });
  auto hold=loop.hold();loop.run();hold.release();worker.join();
  assert(atomicResult==17);
  assert(order==std::vector<std::string>({"first","second","microtask"}));

  std::stop_source cancelled;
  cancelled.request_stop();
  bool invoked=false,aborted=false;
  std::thread cancelledWorker([&]{
    try{(void)query::invokeOwnerSegment(dispatcher,cancelled.get_token(),[&]{
      invoked=true;return 1;
    });}catch(const transport::APIUserAbortError&){aborted=true;}
  });
  cancelledWorker.join();loop.runReady();
  assert(aborted&&!invoked);

  model::PreflightInputs input;
  input.options={{"model","batbox-model"},{"querySource","sdk"}};
  model::QueryServices services;
  services.preflight.isSubscriber=[](){return false;};
  services.preflight.isNonCustomPrimaryModel=[](const std::string&){return false;};
  services.preflight.getAPIProvider=[](){return std::string("local");};
  services.reducer.randomUUID=[](){return std::string("uuid");};
  services.reducer.timestamp=[](){return std::string("now");};
  services.reducer.calculateUSDCost=[](const std::string&,const Json&){return 0.0;};
  services.reducer.addToTotalFlightCost=[](double,const Json&,const std::string&){return 0.0;};
  services.createClient=[]()->std::shared_ptr<transport::Client>{
    throw std::logic_error("client created before builder");};
  services.normalizeFallbackContent=[](const Json& content){return content;};
  services.getAssistantMessageFromError=[](const api_errors::Failure&,const std::string&){
    return Json::object();};
  query::OwnerPreflightAwaits awaits;
  awaits.remainingSegment=[&](const model::PreflightInputs&,
      const model::PreflightServices&,model::PreflightState seed){
    assert(loop.isOwnerThread());order.push_back("preflight-a");
    loop.queueMicrotask([&]{order.push_back("preflight-microtask");});
    order.push_back("preflight-b");
    return seed;
  };
  services.buildParamsSegment=[&](const Json&,requests::RequestCapture&,
      const requests::BuilderDependencies&,const requests::WireProtocol&,
      const requests::BuilderHeaders&)->Json{
    assert(loop.isOwnerThread());order.push_back("builder");
    throw std::runtime_error("builder sentinel");
  };
  auto cursor=query::makeOwnerSegmentQueryModelCursor(loop,input,
    std::move(services),awaits);
  assert(order==std::vector<std::string>({"first","second","microtask"}));
  bool builderFailed=false;
  try{(void)await(loop,cursor.next());}
  catch(const std::runtime_error& error){
    builderFailed=std::string(error.what())=="builder sentinel";
  }
  assert(builderFailed);
  const std::vector<std::string> suffix(order.begin()+3,order.end());
  assert(suffix==std::vector<std::string>({"preflight-a","preflight-b",
    "preflight-microtask","builder"}));

  std::vector<std::string> delayed;
  event_loop::Deferred<bool> configGate(loop);
  model::PreflightServices configImports;
  configImports.isSubscriber=[](){return false;};
  configImports.isNonCustomPrimaryModel=[](const std::string&){return true;};
  configImports.getAPIProvider=[](){return std::string("local");};
  query::OwnerPreflightAwaits configAwaits;
  configAwaits.offSwitchActivated=[&]{
    delayed.push_back("config-await");return configGate;};
  configAwaits.remainingSegment=[&](const model::PreflightInputs&,
      const model::PreflightServices&,model::PreflightState seed){
    delayed.push_back("config-remaining");return seed;};
  auto configPending=query::startOwnerPreflight(loop,input,configImports,configAwaits);
  auto heartbeat=loop.setTimeout(1,[&]{delayed.push_back("config-heartbeat");});
  auto release=loop.setTimeout(20,[&]{configGate.resolve(false);});
  const auto configResult=await(loop,configPending);
  assert(!configResult.offSwitch);
  assert(delayed==std::vector<std::string>({"config-await",
    "config-heartbeat","config-remaining"}));

  delayed.clear();
  event_loop::Deferred<std::optional<std::string>> profileGate(loop);
  model::PreflightInputs profileInput=input;
  profileInput.options["model"]="application-inference-profile/test";
  model::PreflightServices profileImports;
  profileImports.isSubscriber=[](){return true;};
  profileImports.isNonCustomPrimaryModel=[](const std::string&){
    throw std::logic_error("subscriber should skip model gate");return false;};
  profileImports.getAPIProvider=[](){return std::string("bedrock");};
  query::OwnerPreflightAwaits profileAwaits;
  profileAwaits.inferenceProfileBackingModel=[&](const std::string&){
    delayed.push_back("profile-await");return profileGate;};
  profileAwaits.remainingSegment=[&](const model::PreflightInputs&,
      const model::PreflightServices&,model::PreflightState seed){
    delayed.push_back("profile-remaining");return seed;};
  auto profilePending=query::startOwnerPreflight(loop,profileInput,
    profileImports,profileAwaits);
  auto profileHeartbeat=loop.setTimeout(1,[&]{delayed.push_back("profile-heartbeat");});
  auto profileRelease=loop.setTimeout(20,[&]{
    profileGate.resolve(std::optional<std::string>("backing-model"));});
  const auto profileResult=await(loop,profilePending);
  assert(profileResult.resolvedModel=="backing-model");
  assert(delayed==std::vector<std::string>({"profile-await",
    "profile-heartbeat","profile-remaining"}));

  delayed.clear();
  event_loop::Deferred<bool> searchGate(loop);
  model::PreflightServices searchImports;
  searchImports.isSubscriber=[](){return true;};
  searchImports.isNonCustomPrimaryModel=[](const std::string&){return false;};
  searchImports.getAPIProvider=[](){return std::string("local");};
  searchImports.getMergedBetas=[](const std::string&,bool){return Json::array();};
  searchImports.isAdvisorEnabled=[](){return false;};
  query::OwnerPreflightAwaits searchAwaits;
  searchAwaits.gadgetSearchEnabled=[&](const std::string&,const Json&,const Json&){
    delayed.push_back("search-await");return searchGate;};
  searchAwaits.afterGadgetSearchSegment=[&](const model::PreflightInputs&,
      const model::PreflightServices&,model::PreflightState seed,bool enabled){
    delayed.push_back("search-remaining");seed.useGadgetSearch=enabled;return seed;};
  auto searchPending=query::startOwnerPreflight(loop,input,searchImports,searchAwaits);
  auto searchHeartbeat=loop.setTimeout(1,[&]{delayed.push_back("search-heartbeat");});
  auto searchRelease=loop.setTimeout(20,[&]{searchGate.resolve(true);});
  const auto searchResult=await(loop,searchPending);
  assert(searchResult.useGadgetSearch);
  assert(delayed==std::vector<std::string>({"search-await",
    "search-heartbeat","search-remaining"}));
}
