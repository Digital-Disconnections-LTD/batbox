#include <cstdlib>
#include "gadgets/cron/CronBindings.hpp"
#include "bootstrap/BootstrapState.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>

using namespace batbox;
namespace {
template<class T>T wait(event_loop::Loop& loop,std::future<T>& result){
  const auto end=std::chrono::steady_clock::now()+
    std::chrono::seconds(5);
  while(std::chrono::steady_clock::now()<end){
    loop.runReady();
    if(result.wait_for(std::chrono::milliseconds(0))==
       std::future_status::ready)return result.get();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  throw std::runtime_error("Cron native tool bridge timed out");
}
std::future<gadgets::GadgetResult> call(const gadgets::Gadget& gadget,
  const Json& input){
  return std::async(std::launch::async,[&gadget,input]{
    return gadget.call(input,gadgets::GadgetCallContext{},
      std::function<std::future<Json>(const Json&)>{},Json(),
      [](const gadgets::GadgetProgress&){}).get();
  });
}
std::future<gadgets::ValidationResult> validate(const gadgets::Gadget& gadget,
  const Json& input){
  return std::async(std::launch::async,[&gadget,input]{
    return gadget.validateInput(input,Json()).get();
  });
}
}
int main(){
  char pattern[]="/tmp/batbox-cron-tools-XXXXXX";
  const auto root=::mkdtemp(pattern);
  if(!root)throw std::runtime_error("mkdtemp failed");
  event_loop::Loop loop;
  auto state=std::make_shared<bootstrap::BootstrapState>();
  state->identity.directories.projectRoot=root;
  state->identity.flightId="owner";
  std::vector<std::string> errors;
  auto missions=cron::makeIndigenousCronMissionServices(loop,state,
    [&errors](std::exception_ptr error){
      try{std::rethrow_exception(error);}
      catch(const std::exception& ex){errors.push_back(ex.what());}
    },[](const std::string&){});
  gadgets::cron::CronGadgetUI ui;
  ui.renderCreateResult=[](const Json&,const Json&,const Json&){return Json();};
  ui.renderDeleteResult=ui.renderCreateResult;
  ui.renderListResult=ui.renderCreateResult;
  auto configuration=cron::config::indigenousCronConfigurationServices(
    [](const std::string&,const Json& fallback,double){return fallback;},true);
  bat_context::PartnerIdentityState identity;
  auto intlStash=std::make_shared<intl::Stash>(intl::indigenousServices());
  auto widths=std::make_shared<text_width::StringWidth>(
    text_width::indigenousServices(intlStash));
  auto services=gadgets::cron::composeIndigenousCronGadgetServices(loop,state,missions,
    std::move(configuration),identity,
    format::indigenousTruncateServices(intlStash,widths),ui);
  assert(services.truncate("一二三",5,false)=="一二…");
  const auto create=gadgets::cron::makeCronCreateGadget(services);
  const auto remove=gadgets::cron::makeCronDeleteGadget(services);
  const auto list=gadgets::cron::makeCronListGadget(services);
  const Json first={{"cron","* * * * *"},{"prompt","session"},
    {"recurring",false},{"durable",false}};
  auto check=validate(create,first);
  assert(wait(loop,check).result);
  auto created=call(create,first);
  const auto firstResult=wait(loop,created).data;
  const auto flightId=firstResult.at("id").get<std::string>();
  assert(flightId.size()==8);
  assert(bootstrap::getScheduledMissionsEnabled(state->flags));
  auto listing=call(list,Json::object());
  auto jobs=wait(loop,listing).data.at("jobs");
  assert(jobs.size()==1&&jobs[0].at("id")==flightId);
  assert(jobs[0].at("durable")==false);
  const Json durable={{"cron","*/5 * * * *"},{"prompt","file"},
    {"recurring",true},{"durable",true}};
  auto second=call(create,durable);
  const auto durableId=wait(loop,second).data.at("id").get<std::string>();
  const auto path=std::filesystem::path(root)/".batbox"/
    "scheduled_tasks.json";
  assert(std::filesystem::exists(path));
  listing=call(list,Json::object());
  jobs=wait(loop,listing).data.at("jobs");
  assert(jobs.size()==2&&jobs[0].at("id")==durableId);
  assert(jobs[1].at("id")==flightId);
  auto deleteFlight=call(remove,Json{{"id",flightId}});
  assert(wait(loop,deleteFlight).data.at("id")==flightId);
  auto deleteDurable=call(remove,Json{{"id",durableId}});
  assert(wait(loop,deleteDurable).data.at("id")==durableId);
  listing=call(list,Json::object());
  jobs=wait(loop,listing).data.at("jobs");
  assert(jobs.empty()&&errors.empty());
  std::filesystem::remove_all(root);
  std::cout<<"native Cron tool/session/file lifecycle passed\n";
}
