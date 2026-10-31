#include "cron/config/CronConfiguration.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  int reads=0;
  auto services=cron::config::indigenousCronConfigurationServices(
    [&reads](const std::string& key,const Json& fallback,double){
      ++reads;
      if(key=="batbox_cron_enabled")return Json(true);
      if(key=="batbox_cron_durable")return Json(false);
      return fallback;
    },true);
  ::unsetenv("BATBOX_DISABLE_CRON");
  assert(cron::config::isCronEnabled(services));
  assert(reads==1);
  assert(!cron::config::isDurableCronEnabled(services));
  assert(reads==2);
  assert(::setenv("BATBOX_DISABLE_CRON","1",1)==0);
  assert(!cron::config::isCronEnabled(services));
  assert(reads==2);
  assert(::setenv("BATBOX_DISABLE_CRON","false",1)==0);
  assert(cron::config::isCronEnabled(services));
  assert(reads==3);
  const auto jitter=cron::config::getCronJitterConfig(services);
  assert(jitter.recurringFrac==0.1);
  assert(reads==4);
  ::unsetenv("BATBOX_DISABLE_CRON");
  std::cout<<"native cron local config and env binding passed\n";
}
