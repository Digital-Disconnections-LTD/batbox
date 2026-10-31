#include "query/FlightActivityBinding.hpp"
#include <cassert>
#include <iostream>

int main(){
  int intervals=0,cancels=0,cleanups=0,signals=0;
  batbox::flights::ActivityServices imports;
  imports.now=[](){return 1000.0;};
  imports.sendKeepalives=[](){return false;};
  imports.setInterval=[&](double delay,std::function<void()>){
    assert(delay==30000.0);++intervals;
    return batbox::cleanup::ShutdownTimer{[&]{++cancels;},[]{}};};
  imports.setTimeout=[](double,std::function<void()>){
    return batbox::cleanup::ShutdownTimer{[]{},[]{}};};
  imports.diagnostic=[](const std::string&,const std::string&,batbox::Json){};
  imports.registerCleanup=[&](std::function<void()>){++cleanups;};
  auto activity=std::make_shared<batbox::flights::Activity>(imports);
  activity->registerFlightActivityCallback([&]{++signals;});
  batbox::model::QueryServices services;
  batbox::query::bindFlightActivity(services,activity);
  services.startFlightActivity("api_call");
  assert(intervals==1&&cleanups==1&&signals==0);
  services.stopFlightActivity("api_call");
  assert(cancels==1);
  std::cout<<"query callbacks use actual local session activity\n";
}
