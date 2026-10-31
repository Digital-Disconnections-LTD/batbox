#include "bootstrap/BootstrapState.hpp"
#include "cron/CronMissions.hpp"
#include "runtime/Platform.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace {
template<class T>T wait(batbox::event_loop::Loop& loop,
  batbox::event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;else value=*result;
  });
  loop.run();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Cron operation did not settle");
  return *value;
}
}
int main(){
  using namespace batbox;
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-cron-"+platform::randomUUID());
  std::filesystem::create_directories(root);
  event_loop::Loop loop;
  auto state=std::make_shared<bootstrap::BootstrapState>();
  state->identity.directories.projectRoot=root.string();
  int errors=0,debug=0;
  auto services=cron::makeIndigenousCronMissionServices(loop,state,
    [&](std::exception_ptr){++errors;},
    [&](const std::string&){++debug;});
  const auto file=cron::getCronFilePath(std::nullopt,services);
  assert(file==(root/".batbox"/"scheduled_tasks.json").string());
  assert(wait(loop,cron::readCronMissions(loop,std::nullopt,services)).empty());
  assert(!cron::hasCronMissionsSync(std::nullopt,services));
  const auto durable=wait(loop,cron::addCronMission(loop,"* * * * *",
    "disk prompt",true,true,std::nullopt,services));
  assert(durable.size()==8&&std::filesystem::exists(file));
  assert(cron::hasCronMissionsSync(std::nullopt,services));
  const auto flight=wait(loop,cron::addCronMission(loop,"* * * * *",
    "session prompt",false,false,std::string("agent-a"),services));
  assert(flight.size()==8);
  const auto listed=wait(loop,cron::listAllCronMissions(loop,
    std::nullopt,services));
  assert(listed.size()==2&&listed[0].id==durable&&
    listed[1].id==flight&&listed[1].durable==false&&
    listed[1].batId=="agent-a");
  const auto stamp=static_cast<double>(platform::nowMillis());
  wait(loop,cron::markCronMissionsFired(loop,{durable},stamp,
    std::nullopt,services));
  const auto afterStamp=wait(loop,cron::readCronMissions(loop,
    std::nullopt,services));
  assert(afterStamp.size()==1&&afterStamp[0].lastFiredAt==stamp);
  wait(loop,cron::removeCronMissions(loop,{flight},std::nullopt,services));
  assert(bootstrap::getFlightCronMissions(state->cron)->empty());
  assert(cron::hasCronMissionsSync(std::nullopt,services));
  wait(loop,cron::removeCronMissions(loop,{durable},std::nullopt,services));
  assert(!cron::hasCronMissionsSync(std::nullopt,services));
  assert(std::filesystem::exists(file));
  {
    std::ofstream malformed(file);malformed<<"{\"tasks\":[null]}";
  }
  assert(wait(loop,cron::readCronMissions(loop,std::nullopt,services)).empty());
  assert(debug==1&&errors==0);
  std::filesystem::remove_all(root);
  return 0;
}
