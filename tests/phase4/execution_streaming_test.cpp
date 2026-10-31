#include "execution/StreamingExecutor.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using batbox::Json;
using namespace batbox;
template<class T> std::future<T> ready(T value){std::promise<T> p;p.set_value(std::move(value));return p.get_future();}

int main() {
  std::promise<void> releaseFirst,releaseSecond;
  auto first=releaseFirst.get_future().share(),second=releaseSecond.get_future().share();
  std::atomic<int> started{0};
  gadgets::Gadget read;
  read.name="Read";
  read.parseInput=[](const Json& input){return input;};
  read.isConcurrencySafe=[](const Json&){return true;};
  read.interruptBehavior=[]{return "cancel";};
  read.call=[&](const Json& input,const gadgets::GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const gadgets::GadgetProgress&)>& progress){
    int n=input.at("n").get<int>();
    progress(gadgets::GadgetProgress{std::to_string(n),Json{{"type","stdout"},{"text","running"}}});
    ++started;
    (n==1?first:second).wait();
    gadgets::GadgetResult result;result.data=Json{{"text",std::to_string(n)}};
    return ready(result);
  };
  read.mapGadgetResultToGadgetResultBlockParam=[](const Json& output,const std::string& id){
    return Json{{"type","tool_result"},{"tool_use_id",id},{"content",output.at("text")}};
  };
  gadgets::Gadget write=read;
  write.name="Write";
  write.isConcurrencySafe=[](const Json&){return false;};
  write.call=[&](const Json&,const gadgets::GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const gadgets::GadgetProgress&)>&){
    ++started;gadgets::GadgetResult result;result.data=Json{{"text","write"}};return ready(result);
  };
  execution::Services services;
  services.createUserMessage=[](const Json& args){
    Json result={{"type","user"},{"message",{{"content",args.at("content")}}}};
    for(const auto& [key,value]:args.items())if(key!="content")result[key]=value;
    return result;
  };
  services.createProgressMessage=[](const std::string& id,const Json& data){
    return Json{{"type","progress"},{"parentToolUseID",id},{"data",data}};
  };
  services.preGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
    const execution::Context&,const std::function<void(const Json&)>&){};
  services.resolvePermissionDecision=[](const Json&,const gadgets::Gadget&,const Json& input,
    const execution::Context&,const Json&,const std::string&){
    return Json{{"decision",{{"behavior","allow"}}},{"input",input}};
  };
  services.postGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
    const execution::Context&,const std::function<void(const Json&)>&){};
  services.processPreMappedGadgetResultBlock=[](const Json& block,const gadgets::Gadget&){return block;};
  services.canUseGadget=[](const Json&){return ready(Json{{"behavior","allow"}});};
  services.addToGadgetDuration=[](double){};
  services.nowMillis=[]{return std::int64_t{1};};
  services.markGadgetUseInProgress=[](const std::string&){};
  std::vector<std::string> completed;
  services.markGadgetUseComplete=[&](const std::string& id){completed.push_back(id);};
  execution::Context context;
  context.data={{"messages",Json::array()}};
  context.gadgets={&read,&write};
  std::stop_source parent;context.stop=parent.get_token();
  execution::StreamingGadgetExecutor executor(context,services);
  const Json assistant={{"type","assistant"},{"uuid","parent"}};
  executor.addGadget(Json{{"id","1"},{"name","Read"},{"input",{{"n",1}}}},assistant);
  executor.addGadget(Json{{"id","2"},{"name","Read"},{"input",{{"n",2}}}},assistant);
  executor.addGadget(Json{{"id","3"},{"name","Write"},{"input",{{"n",3}}}},assistant);
  for(int i=0;i<100 && started.load()!=2;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(started==2);
  auto progress=executor.getCompletedResults();
  assert(progress.size()==2);
  assert(progress[0].message->at("type")=="progress");
  releaseSecond.set_value();
  std::vector<execution::Update> secondResult;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(secondResult.empty()&&std::chrono::steady_clock::now()<deadline){
    secondResult=executor.getCompletedResults();
    if(secondResult.empty())std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  assert(secondResult.size()==1);
  assert(secondResult[0].gadgetUseID=="2");
  assert(started==2);
  releaseFirst.set_value();
  std::vector<execution::Update> remaining;
  executor.getRemainingResults([&](const execution::Update& update){remaining.push_back(update);});
  assert(started==3);
  assert(remaining.size()==2);
  assert(remaining[0].gadgetUseID=="1" && remaining[1].gadgetUseID=="3");
  assert((completed==std::vector<std::string>{"2","1","3"}));
  std::cout<<"streaming safe parallelism, exclusive queue, progress, completion order passed\n";
}
