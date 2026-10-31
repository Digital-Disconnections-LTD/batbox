#include <unistd.h>
#include "settings/watcher/IndigenousWatcher.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
void need(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
void write(const std::string& path,const std::string& content){
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  if(!out)throw std::runtime_error("Cannot open watcher fixture");
  out<<content;out.close();
}
}
int main(){
  try{
    char directory[]="/tmp/batbox-native-watch-XXXXXX";
    need(::mkdtemp(directory)!=nullptr,"mkdtemp failed");
    const auto root=std::filesystem::path(directory);
    const auto file=(root/"settings.json").string();
    const auto sibling=(root/"settings.local.json").string();
    const auto dropin=root/"managed-settings.d";
    std::filesystem::create_directory(dropin);
    const auto fragment=(dropin/"fragment.json").string();
    const auto temp=(root/"temp.json").string();
    write(file,"old");
    batbox::event_loop::Loop owner;
    batbox::settings::persistence::WatchTargets targets;
    targets.dirs={root.string(),dropin.string()};
    targets.settingsFiles={file,sibling};targets.dropInDir=dropin.string();
    std::vector<std::string> events;
    batbox::settings::watcher::Events handlers;
    handlers.add=[&](const std::string& path){events.push_back("add:"+path);};
    handlers.change=[&](const std::string& path){events.push_back("change:"+path);};
    handlers.unlink=[&](const std::string& path){events.push_back("unlink:"+path);};
    handlers.error=[&](const std::string& error){events.push_back("error:"+error);};
    batbox::settings::watcher::IndigenousWatcher watcher(targets,
      owner.dispatcher(),handlers,{80,20,40});
    auto pump=[&](int milliseconds){
      const auto until=std::chrono::steady_clock::now()+
        std::chrono::milliseconds(milliseconds);
      while(std::chrono::steady_clock::now()<until){
        owner.runReady();std::this_thread::sleep_for(10ms);
      }
      owner.runReady();
    };
    auto expect=[&](const std::string& kind,const std::string& path,
        std::size_t before){
      for(int i=0;i<200;i++){
        owner.runReady();
        for(std::size_t j=before;j<events.size();j++)
          if(events[j]==kind+":"+path)return;
        std::this_thread::sleep_for(10ms);
      }
      throw std::runtime_error("Missing native watcher event: "+kind);
    };
    pump(160);need(events.empty(),"Initial file was emitted");
    write(root/"ignored.txt","ignored");
    pump(160);need(events.empty(),"Ignored file was emitted");
    auto before=events.size();write(sibling,"new sibling");
    expect("add",sibling,before);
    before=events.size();write(fragment,"{\"policy\":true}");
    expect("add",fragment,before);
    before=events.size();write(file,"changed-content");
    expect("change",file,before);
    before=events.size();write(temp,"replacement");
    std::filesystem::rename(temp,file);expect("change",file,before);
    before=events.size();std::filesystem::remove(file);
    write(file,"recreated-fast");expect("change",file,before);
    before=events.size();std::filesystem::remove(file);
    expect("unlink",file,before);
    before=events.size();write(file,"recreated-late");
    expect("add",file,before);
    const auto moved=root/"moved-drop-in";
    std::filesystem::rename(dropin,moved);
    std::filesystem::create_directory(dropin);
    pump(160);
    before=events.size();
    const auto replacementFragment=(dropin/"after-recreate.json").string();
    write(replacementFragment,"{\"restored\":true}");
    expect("add",replacementFragment,before);
    watcher.close();before=events.size();
    write(file,"after-close");pump(160);
    need(events.size()==before,"Event emitted after watcher close");
    std::filesystem::remove_all(root);
    std::cout<<batbox::value::stringify({{"events",events.size()},
      {"initialSuppressed",true},{"ignoredSuppressed",true},
      {"closeSuppressed",true}})<<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
