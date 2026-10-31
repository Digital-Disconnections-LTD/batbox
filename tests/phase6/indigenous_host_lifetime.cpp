#include "application/HostInternal.hpp"
#include "runtime/Platform.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace batbox::application {
struct HostLifetimeProbe {
  static void check(bool value,const char* message){
    if(!value)throw std::logic_error(message);
  }
  static void run(){
    const auto original=std::filesystem::current_path();
    const auto root=std::filesystem::temp_directory_path()/
      ("batbox-host-lifetime-"+platform::randomUUID());
    const auto first=root/"first",second=root/"second";
    std::filesystem::create_directories(first);
    std::filesystem::create_directories(second);
    try{
      Config config;
      config.cwd=first;config.stateDirectory=root/"state";
      config.configDirectory=root/"config";
      config.configPath=config.configDirectory/"config.json";
      config.model="offline-fixture";
      config.baseUrl="http://127.0.0.1:1";
      config.print=true;
      Host host(config);
      auto& impl=*host.impl_;
      impl.initialize();
      try{
      const std::string oldId="a1234567";
      const auto oldFlight=impl.flight->id();
      impl.bats->restoreTranscriptOrigins({{oldId,oldFlight}});
      impl.store->setState([&](const app_state::AppStateRef& previous){
        auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
        Json missions=value::property(previous->snapshot(),"tasks");
        if(!missions.is_object())missions=Json::object();
        missions[oldId]={{"type","local_agent"},{"agentType","general-purpose"},
          {"status","completed"},{"retain",false},{"diskLoaded",false},
          {"description","archived child"}};
        next->fields->set("tasks",missions);
        return next;
      });
      const auto originalTranscript=impl.bats->transcriptPath(oldId);
      const Json firstMessage={{"type","user"},{"uuid","fixture-user"},
        {"parentUuid",nullptr},{"agentId",oldId},{"isSidechain",true},
        {"timestamp","2026-10-30T12:00:00.000Z"},
        {"message",{{"role","user"},{"content","Original child prompt"}}}};
      const Json secondMessage={{"type","assistant"},{"uuid","fixture-assistant"},
        {"parentUuid","fixture-user"},{"agentId",oldId},{"isSidechain",true},
        {"timestamp","2026-10-30T12:00:01.000Z"},
        {"message",{{"id","fixture-response"},{"role","assistant"},
          {"model","offline-fixture"},
          {"content",Json::array({Json{{"type","text"},
            {"text","Original child reply"}}})}}}};
      std::filesystem::create_directories(
        std::filesystem::path(originalTranscript).parent_path());
      {std::ofstream file(originalTranscript,std::ios::binary);
        file<<firstMessage.dump()<<'\n'<<secondMessage.dump()<<'\n';}
      auto stashSafe=std::make_shared<bats::StashSafeParams>();
      stashSafe->gadgetUseContext.query.gadgetSnapshotOwner=impl.roster->current();
      bats::saveStashSafeParams(stashSafe);stashSafe.reset();
      std::weak_ptr<application_bats::IndigenousBatApplication> oldBat=impl.bats;
      std::weak_ptr<application_poetry::Provider> oldPoetry=impl.poetry;
      std::weak_ptr<gadgets::live_roster::Roster> oldRoster=impl.roster;
      std::weak_ptr<const gadgets::live_roster::Snapshot> oldSnapshot=
        impl.roster->current();
      const auto previous=impl.roost->snapshot();
      application_roost::Snapshot next{std::filesystem::canonical(second),
        std::filesystem::canonical(second),Json()};
      // Hold the retirement pass until the new generation owns a running
      // child. The old provider's shutdown must leave that child's abort
      // source alone even though both providers share the AppState store.
      impl.bindingRetired=true;
      impl.roost->activate({next,false},[&](const auto& before,const auto& after){
        impl.rebindRoost(before,after);});
      check(impl.roost->cwd()==next.cwd,"Active workspace did not switch");
      auto liveBat=impl.bats;
      auto livePoetry=impl.poetry;
      check(liveBat!=oldBat.lock(),"Rebind reused old agent owner");
      auto activeServices=liveBat->makeServices();
      bats::BatRegistrationParams newMission;
      newMission.batId="a7654321";newMission.description="new workspace child";
      newMission.poetry="offline fixture";
      newMission.selectedBat={{"agentType","general-purpose"}};
      const auto activeChild=bats::registerAsyncBat(newMission,
        *activeServices.async.registration);
      check(!activeChild.abortOwner->stop_requested(),
        "New child was stopped before old workspace shutdown");
      impl.bindingRetired=false;
      impl.releaseRetiredProviders();
      const auto until=[&](const std::function<bool()>& predicate){
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        std::function<void()> poll;
        poll=[&]{
          if(predicate()||std::chrono::steady_clock::now()>deadline){impl.owner.stop();return;}
          impl.owner.setTimeout(20,poll,true);
        };
        impl.owner.setTimeout(20,poll,true);
        auto hold=impl.owner.hold();impl.owner.run();hold.release();
        check(predicate(),"Retired Roost did not release its providers");
      };
      until([&]{return impl.retiredRoosts.size()==1&&
        impl.retiredRoosts[0].state==Host::Impl::RetiredRoost::State::Dormant&&
        oldBat.expired()&&oldPoetry.expired()&&oldRoster.expired()&&oldSnapshot.expired();});
      check(oldBat.expired()&&oldPoetry.expired()&&oldRoster.expired()&&
        oldSnapshot.expired(),
        "Dormant workspace retained old heavy providers");
      check(!activeChild.abortOwner->stop_requested()&&
        value::property(value::property(impl.appState(),"tasks"),newMission.batId)
          .value("status",std::string{})=="running",
        "Retired workspace shutdown stopped an active workspace child");
      const auto summary=impl.runtimeMemorySummary();
      check(summary.at("dormantWorkspaceRecords")==1&&
        summary.at("liveWorkspaceGenerations")==1,
        "Runtime summary did not count dormant identity");
      Json observed;std::exception_ptr failure;
      impl.observeBatMission(oldId).then([&](const Json* mission,
          const std::exception_ptr& error){
        failure=error;if(mission)observed=*mission;impl.owner.stop();});
      check(impl.retiredRoosts[0].state==
        Host::Impl::RetiredRoost::State::Live,
        "Old child provider did not revive when observation began");
      auto hold=impl.owner.hold();impl.owner.run();hold.release();
      if(failure)std::rethrow_exception(failure);
      check(observed.is_object()&&observed.value("diskLoaded",false),
        "Old child could not be observed after workspace revival");
      const auto& messages=value::property(observed,"messages");
      check(messages.is_array()&&messages.size()==2&&
        value::property(value::property(messages[0],"message"),"content")==
          "Original child prompt"&&
        value::property(value::property(messages[1],"message"),"content")[0]["text"]==
          "Original child reply",
        "Revived child did not load its original disk transcript");
      auto revived=impl.batOwner(oldId);
      check(revived&&revived->parentFlightId(oldId)==oldFlight&&
        revived->transcriptPath(oldId)==originalTranscript,
        "Revived child lost original session or project path");
      check(impl.bats==liveBat&&impl.poetry==livePoetry&&
        impl.roost->cwd()==next.cwd,
        "Old child observation changed the active workspace");
      impl.requestExit();
      impl.owner.run();
      // stop() can leave already settled promise observers queued. They may
      // own providers whose destructors still need this event loop.
      impl.owner.runReady();
      (void)previous;
      }catch(...){
        impl.requestExit();
        impl.owner.run();
        impl.owner.runReady();
        throw;
      }
    }catch(...){
      std::filesystem::current_path(original);
      std::filesystem::remove_all(root);
      throw;
    }
    std::filesystem::current_path(original);
    std::filesystem::remove_all(root);
  }
};
}
int main(){
  batbox::application::HostLifetimeProbe::run();
  std::cout<<"native host dormant workspace lifetime passed\n";
}
