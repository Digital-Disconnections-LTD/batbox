#include "application/HostInternal.hpp"
#include "flights/SavedBats.hpp"
#include "runtime/Platform.hpp"
#include "user_input/SlashInternal.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace batbox::application {
struct HostSavedBatsProbe {
  static void check(bool value,const std::string& message){
    if(!value)throw std::logic_error(message);
  }
  static Json row(const std::string& id,const std::string& type,const std::string& uuid,
      const Json& parent,const std::string& at,const Json& content){
    Json message={{"role",type},{"content",content}};
    if(type=="assistant"){message["id"]="response-"+uuid;message["model"]="offline-fixture";
      message["usage"]={{"input_tokens",40},{"output_tokens",8}};}
    return Json{{"type",type},{"uuid",uuid},{"parentUuid",parent},{"agentId",id},
      {"isSidechain",true},{"timestamp",at},{"message",std::move(message)}};
  }
  static void writeBat(const std::filesystem::path& directory,const std::string& id,
      const std::string& description,const std::string& reply){
    std::filesystem::create_directories(directory);
    std::ofstream file(directory/("agent-"+id+".jsonl"),std::ios::binary);
    file<<row(id,"user","u-"+id,nullptr,"2026-10-30T12:00:00.000Z","Saved prompt for "+id).dump()<<'\n';
    file<<row(id,"assistant","a-"+id,"u-"+id,"2026-10-30T12:00:02.000Z",
      Json::array({Json{{"type","text"},{"text","Looking"}},
        Json{{"type","tool_use"},{"id","g-"+id},{"name","Read"},{"input",Json::object()}}})).dump()<<'\n';
    file<<row(id,"user","r-"+id,"a-"+id,"2026-10-30T12:00:03.000Z",
      Json::array({Json{{"type","tool_result"},{"tool_use_id","g-"+id},{"content","ok"}}})).dump()<<'\n';
    file<<row(id,"assistant","f-"+id,"r-"+id,"2026-10-30T12:00:05.500Z",
      Json::array({Json{{"type","text"},{"text",reply}}})).dump()<<'\n';
    std::ofstream(directory/("agent-"+id+".meta.json"),std::ios::binary)<<
      Json{{"agentType","Explore"},{"description",description}}.dump();
  }
  template<class T>static T settle(Host::Impl& impl,event_loop::Deferred<T> pending){
    std::optional<T> value;std::exception_ptr failure;
    pending.then([&](const T* result,const std::exception_ptr& error){
      failure=error;if(result)value=*result;impl.owner.stop();});
    auto hold=impl.owner.hold();impl.owner.run();hold.release();
    if(failure)std::rethrow_exception(failure);
    check(value.has_value(),"Deferred did not settle");
    return *value;
  }
  static void runFor(Host::Impl& impl,double milliseconds){
    impl.owner.setTimeout(milliseconds,[&impl]{impl.owner.stop();});
    auto hold=impl.owner.hold();impl.owner.run();hold.release();
  }
  static void setMission(Host::Impl& impl,const std::string& id,const Json& mission){
    impl.store->setState([&](const app_state::AppStateRef& previous){
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      Json missions=value::property(previous->snapshot(),"tasks");
      if(!missions.is_object())missions=Json::object();
      missions[id]=mission;
      next->fields->set("tasks",missions);
      return next;
    });
  }
  static const Json& mission(const Json& state,const std::string& id){
    return value::property(value::property(state,"tasks"),id);
  }

  static void commandNames(Host::Impl& impl){
    const auto& entries=impl.commands->entries();
    for(const auto* name:{"bats","agents","missions","tasks","bashes"})
      check(user_input::internal::findEntry(name,entries)!=nullptr,
        std::string("/")+name+" must resolve to a live command");
    check(user_input::internal::nameOf(*user_input::internal::findEntry("agents",entries))=="bats"&&
      user_input::internal::nameOf(*user_input::internal::findEntry("tasks",entries))=="missions",
      "Legacy names must alias the Bats and Missions commands");
    for(const auto* name:{"agents","bats","/missions","tasks","bashes"}){
      check(impl.commands->openUiCommand(name,""),
        std::string("Control command lookup must accept ")+name);
      for(int i=0;i<5;++i)impl.owner.runReady();
      check(impl.commands->router()->ui()->hasActionableUi(),
        std::string(name)+" must open its dialog");
      impl.commands->router()->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
      for(int i=0;i<5;++i)impl.owner.runReady();
    }
    check(!impl.commands->openUiCommand("not-a-command",""),
      "Unknown control commands must report failure");
  }

  static void savedBats(Host::Impl& impl){
    const auto subagents=std::filesystem::path(impl.bats->transcriptPath("a0000000")).parent_path();
    writeBat(subagents,"a1111111","Saved one","First saved reply");
    writeBat(subagents/"nested","a2222222","Saved two","Second saved reply");
    writeBat(subagents,"a3333333","Live one","Live reply");
    std::ofstream(subagents/"agent-a9999999.jsonl")<<"not json\n";
    const auto scanned=flights::scanSavedBats(subagents,50);
    check(scanned.size()==3,"Scanner must skip sidechains without readable rows");
    check(flights::savedBatTimestamp("2026-10-30T12:00:05.500Z")-
      flights::savedBatTimestamp("2026-10-30T12:00:00.000Z")==5500,"Timestamp parsing changed");
    setMission(impl,"a3333333",Json{{"type","local_agent"},{"agentType","general-purpose"},
      {"status","running"},{"description","Live one"},{"retain",false}});
    const auto added=settle(impl,impl.bats->seedSavedBats());
    check(added==2,"Exactly the two Bats missing from AppState must be seeded, got "+
      std::to_string(added));
    const auto state=impl.appState();
    const auto& first=mission(state,"a1111111");
    check(first.value("status",std::string{})=="completed"&&first.at("diskLoaded")==false&&
      first.at("description")=="Saved one"&&first.at("agentType")=="Explore"&&
      first.at("model")=="offline-fixture"&&
      first.at("progress").at("lastText")=="First saved reply"&&
      first.at("progress").at("toolUseCount")==1&&
      first.at("prompt")=="Saved prompt for a1111111"&&
      first.at("endTime").get<std::int64_t>()-first.at("startTime").get<std::int64_t>()==5500,
      "Saved Bat record is incomplete: "+first.dump());
    check(mission(state,"a3333333").value("status",std::string{})=="running",
      "Seeding must never replace a live record");
    check(impl.bats->resolveMissionId("a2222222")=="a2222222","Seeded Bats must resolve");
    check(impl.bats->transcriptPath("a2222222")==
        (subagents/"nested"/"agent-a2222222.jsonl").string(),
      "Seeded Bats in a subdirectory must keep their transcript path");
    const auto observed=settle(impl,impl.observeBatMission("a2222222"));
    const auto& messages=value::property(observed,"messages");
    check(observed.value("diskLoaded",false)&&messages.is_array()&&messages.size()==4&&
      value::property(value::property(messages[3],"message"),"content")[0]["text"]==
        "Second saved reply","Opening a seeded Bat must load its saved transcript");
    impl.batOwner("a2222222")->releaseMission("a2222222");
    check(settle(impl,impl.bats->seedSavedBats())==0,"Seeding twice adds nothing");

    // A Flight is listed once: a Bat saved afterwards is not picked up by a
    // later conversation id change for the same Flight.
    writeBat(subagents,"a4444444","Saved later","Later reply");
    check(settle(impl,impl.bats->seedSavedBats())==0&&
      mission(impl.appState(),"a4444444").is_null(),"A Flight's Bats are seeded only once");
    // Results of a scan for a Flight that is no longer current are dropped.
    check(impl.bats->applySavedBats("00000000-0000-4000-8000-000000000000",
        flights::scanSavedBats(subagents,50))==0&&
      mission(impl.appState(),"a4444444").is_null(),
      "Bats scanned for an earlier Flight must not be seeded into the current one");
    check(impl.bats->applySavedBats(impl.flight->id(),flights::scanSavedBats(subagents,50))==1&&
      !mission(impl.appState(),"a4444444").is_null(),"The current Flight's scan still applies");
  }

  // Only the newest sidechains by modification time are opened, and an
  // unreadable one does not use up the limit.
  static void scanLimit(const std::filesystem::path& root){
    const auto directory=root/"scan-limit";
    const auto base=std::filesystem::file_time_type::clock::now();
    for(int i=0;i<5;++i){
      const auto id="b000000"+std::to_string(i);
      writeBat(directory,id,"Bat "+std::to_string(i),"reply");
      std::filesystem::last_write_time(directory/("agent-"+id+".jsonl"),
        base-std::chrono::hours(10-i));
    }
    std::ofstream(directory/"agent-b9999999.jsonl")<<"not json\n";
    std::filesystem::last_write_time(directory/"agent-b9999999.jsonl",base);
    const auto newest=flights::scanSavedBats(directory,2);
    std::vector<std::string> ids;
    for(const auto& bat:newest)ids.push_back(bat.id);
    std::sort(ids.begin(),ids.end());
    check(ids==std::vector<std::string>({"b0000003","b0000004"}),
      "The scan must read the two most recently modified readable sidechains");
    check(flights::scanSavedBats(directory,50).size()==5,"Without a tight limit every Bat is listed");
  }

  static void streamThrottle(Host::Impl& impl){
    const auto running=Json{{"type","local_agent"},{"agentType","general-purpose"},
      {"status","running"},{"description","streaming"},{"retain",true},{"diskLoaded",true},
      {"messages",Json::array()}};
    setMission(impl,"a4444444",running);
    auto quiet=running;quiet["retain"]=false;quiet.erase("messages");
    setMission(impl,"a5555555",quiet);
    std::size_t changes=0;
    auto listener=std::make_shared<app_state::AppStateStore::Listener>([&]{++changes;});
    const auto unsubscribe=impl.store->subscribe(listener);
    const auto delta=[](const std::string& text){
      return Json{{"type","stream_event"},{"event",{{"type","content_block_delta"},{"index",0},
        {"delta",{{"type","text_delta"},{"text",text}}}}}};
    };
    std::string expected;
    for(int i=0;i<30;++i){
      const auto piece="w"+std::to_string(i)+" ";expected+=piece;
      impl.bats->publishStream("a5555555",delta(piece));
    }
    check(changes==0,"An unobserved Bat's stream must never touch AppState");
    check(impl.bats->retainedStateSummary().at("retainedStreamBytes").get<std::size_t>()>=expected.size(),
      "An unobserved Bat's live stream must still be kept for later observation");
    for(int i=0;i<30;++i)impl.bats->publishStream("a4444444",delta("w"+std::to_string(i)+" "));
    check(changes==1,"A burst of deltas must publish once at once, got "+std::to_string(changes));
    runFor(impl,250);
    check(changes==2,"The burst must end with exactly one trailing publish, got "+
      std::to_string(changes));
    check(mission(impl.appState(),"a4444444").value("streamingText",std::string{})==expected,
      "The trailing publish must carry the complete streamed text");
    impl.bats->publishStream("a4444444",Json{{"type","assistant"},{"uuid","final-a4"},
      {"message",{{"role","assistant"},{"content",Json::array({Json{{"type","text"},
        {"text",expected}}})}}}});
    const auto finished=mission(impl.appState(),"a4444444");
    check(changes==3&&value::property(finished,"messages").size()==1&&
      value::property(finished,"streamingText").is_null(),
      "A complete message must publish at once and clear the live text");
    unsubscribe();
  }

  static void sendMessageResult(Host::Impl& impl){
    check(bool(impl.bats),"The host must own a Bat application");
    const auto gadget=impl.bats->makeSendMessageGadget();
    check(bool(gadget.renderGadgetResultMessage),"SendMessage needs a result renderer");
    const Json output={{"success",true},
      {"message","Message queued for delivery to reviewer at its next tool round."}};
    const Json progress=Json::array({Json{{"message","progress must not be shown"}}});
    const auto shown=gadget.renderGadgetResultMessage(output,progress,Json::object());
    check(shown==output.at("message"),
      "SendMessage must render its result output, not its progress: "+shown.dump());
  }

  // /help groups the Bats and Missions commands under Batbox names, and no
  // help text calls a Bat an agent or a Mission a task.
  static void helpVocabulary(Host::Impl& impl){
    check(impl.commands->openUiCommand("help",""),"/help must open");
    for(int i=0;i<5;++i)impl.owner.runReady();
    const auto& shown=impl.commands->router()->ui()->visibleUi();
    check(shown&&shown->id=="help","/help must show its dialog");
    const auto section=[&](const std::string& name)->std::string{
      for(std::size_t i=0;i<shown->choices.size();++i)
        if(shown->choices[i]==name)
          return i<shown->choiceInfo.size()?shown->choiceInfo[i].section:std::string{};
      return "missing";
    };
    check(section("/bats")=="Bats and Missions"&&section("/missions")=="Bats and Missions",
      "/bats and /missions must be listed under Bats and Missions, got '"+
        section("/bats")+"' and '"+section("/missions")+"'");
    for(std::size_t i=0;i<shown->choices.size();++i)if(shown->choices[i]=="/rename")
      check(i<shown->choiceInfo.size()&&shown->choiceInfo[i].detail=="Name this Flight"&&
        shown->choiceInfo[i].section=="Flight","/rename must read \"Name this Flight\"");
    check(section("/rename")=="Flight","/help must list /rename under Flight");
    check(section("/agents")=="missing"&&section("/tasks")=="missing",
      "/help lists commands by their Batbox names, not their aliases");
    std::string text=shown->title+"\n"+shown->body;
    for(std::size_t i=0;i<shown->choices.size();++i){
      text+="\n"+shown->choices[i];
      if(i<shown->choiceInfo.size())
        text+=" "+shown->choiceInfo[i].detail+" "+shown->choiceInfo[i].section;
    }
    for(const char* word:{"Agent","agent","Tasks"," tasks"})
      check(text.find(word)==std::string::npos,std::string("/help must not say '")+word+
        "':\n"+text);
    impl.commands->router()->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
    for(int i=0;i<5;++i)impl.owner.runReady();
    const auto gadget=impl.bats->makeGadget();
    check(gadget.userFacingName&&gadget.userFacingName(Json::object())=="Bat",
      "A Bat without a type must be shown as Bat");
  }

  static void run(){
    const auto original=std::filesystem::current_path();
    const auto root=std::filesystem::temp_directory_path()/
      ("batbox-saved-bats-"+platform::randomUUID());
    std::filesystem::create_directories(root/"work");
    try{
      Config config;
      config.cwd=root/"work";config.stateDirectory=root/"state";
      config.configDirectory=root/"config";
      config.configPath=config.configDirectory/"config.json";
      config.model="offline-fixture";
      config.baseUrl="http://127.0.0.1:1";
      config.print=true;
      Host host(config);
      auto& impl=*host.impl_;
      impl.initialize();
      const auto finish=[&impl]{
        impl.requestExit();impl.owner.run();
        // stop() can leave settled observers queued that own providers.
        impl.owner.runReady();
      };
      try{
        scanLimit(root);
        commandNames(impl);
        savedBats(impl);
        streamThrottle(impl);
        sendMessageResult(impl);
        helpVocabulary(impl);
      }catch(...){finish();throw;}
      finish();
    }catch(...){
      std::filesystem::current_path(original);
      std::filesystem::remove_all(root);throw;
    }
    std::filesystem::current_path(original);
    std::filesystem::remove_all(root);
  }
};
}
int main(){
  try{
    batbox::application::HostSavedBatsProbe::run();
    std::cout<<"Bats commands resolve, saved Bats seed and open, Bat streams publish throttled, "
      "and SendMessage shows its result\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
