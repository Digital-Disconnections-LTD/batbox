#include "bat_context/PartnerContext.hpp"
#include "gadgets/cron/CronGadgets.hpp"
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
void check(bool condition){
  if(!condition)throw std::runtime_error(
    "Cron tool lost registration-time teammate context");
}
std::future<gadgets::GadgetResult> call(const gadgets::Gadget& gadget,const Json& input){
  return gadget.call(input,gadgets::GadgetCallContext{},
    std::function<std::future<Json>(const Json&)>{},Json(),
    [](const gadgets::GadgetProgress&){});
}
template<class T>T getOnWorker(std::future<T> result){
  return std::async(std::launch::async,
    [result=std::move(result)]()mutable{return result.get();}).get();
}
}
int main(){
  std::vector<cron::CronMission> missions;
  cron::CronMission other;
  other.id="other";other.cron="* * * * *";other.poetry="other";
  other.batId="other@team";
  missions.push_back(other);
  gadgets::cron::CronGadgetServices services;
  services.cronEnabled=[]{return true;};
  services.durableEnabled=[]{return true;};
  services.nowMs=[]{return 0.0;};
  services.partnerBatId=[]{
    const auto scoped=bat_context::getPartnerContext();
    return scoped?std::optional(scoped->batId):std::nullopt;
  };
  services.cronFilePath=[]{return std::string("/tmp/batbox-context-test");};
  services.cronToHuman=[](const std::string&){return std::string("Every minute");};
  services.truncate=[](const std::string& text,double,bool){return text;};
  services.listMissions=[&]{return ready(missions);};
  services.addMission=[&](const std::string& expression,
      const std::string& poetry,bool recurring,bool durable,
      const std::optional<std::string>& batId){
    cron::CronMission added;
    added.id="created";added.cron=expression;added.poetry=poetry;
    added.recurring=recurring;added.durable=durable;added.batId=batId;
    missions.push_back(added);
    return ready(std::string("created"));
  };
  services.removeMissions=[](const std::vector<std::string>&){
    return ready(event_loop::Unit{});
  };
  services.setScheduledMissionsEnabled=[](bool){};
  services.ui.renderCreateResult=[](const Json&,const Json&,const Json&){
    return Json();};
  services.ui.renderDeleteResult=services.ui.renderCreateResult;
  services.ui.renderListResult=services.ui.renderCreateResult;
  const auto create=gadgets::cron::makeCronCreateGadget(services);
  const auto remove=gadgets::cron::makeCronDeleteGadget(services);
  const auto list=gadgets::cron::makeCronListGadget(services);
  auto scoped=bat_context::createPartnerContext({"self@team",
    "self","team",std::nullopt,false,"leader",
    std::make_shared<std::stop_source>()});
  const Json input={{"cron","* * * * *"},{"prompt","work"},
    {"durable",false}};
  auto createFuture=bat_context::runWithPartnerContext(scoped,[&]{
    return call(create,input);});
  check(!bat_context::getPartnerContext());
  const auto created=getOnWorker(std::move(createFuture));
  check(created.data.at("id")=="created");
  check(missions.back().batId=="self@team");
  auto listFuture=bat_context::runWithPartnerContext(scoped,[&]{
    return call(list,Json::object());});
  const auto listed=getOnWorker(std::move(listFuture));
  check(listed.data.at("jobs").size()==1);
  check(listed.data.at("jobs")[0].at("id")=="created");
  auto deleteCheck=bat_context::runWithPartnerContext(scoped,[&]{
    return remove.validateInput(Json{{"id","other"}},Json());});
  const auto denied=getOnWorker(std::move(deleteCheck));
  check(!denied.result&&denied.errorCode==2);
  auto createCheck=bat_context::runWithPartnerContext(scoped,[&]{
    return create.validateInput(Json{{"cron","* * * * *"},
      {"prompt","work"},{"durable",true}},Json());});
  const auto durableDenied=getOnWorker(std::move(createCheck));
  check(!durableDenied.result&&durableDenied.errorCode==4);
  std::cout<<"Cron tool deferred futures retain teammate context\n";
}
