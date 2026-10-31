#include "cleanup/Terminal.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace batbox;
namespace {
struct Case {std::string name;bool tty=false,interactive=false,exists=false,
  alt=false,tab=false,unmountError=false,disableTitle=false,windows=false,
  writeError=false;std::optional<std::string> title;};
Json run(const Case& item){
  Json logs=Json::array();cleanup::TerminalServices services;
  services.stdoutIsTTY=[&]{return item.tty;};
  services.isInteractive=[&]{return item.interactive;};
  services.isFlightPersistenceDisabled=[]{return false;};
  services.supportsTabStatus=[&]{return item.tab;};
  services.disableTerminalTitle=[&]{return item.disableTitle;};
  services.isWindows=[&]{return item.windows;};
  services.getInkInstance=[&]() -> std::optional<cleanup::InkShutdownInstance> {
    logs.push_back("getInstance");
    if(!item.alt&&!item.unmountError)return std::nullopt;
    cleanup::InkShutdownInstance instance;
    instance.isAltScreenActive=item.alt;
    instance.unmount=[&]{logs.push_back("unmount");
      if(item.unmountError)throw std::runtime_error("unmount");};
    instance.drainStdin=[&]{logs.push_back("drain");};
    instance.detachForShutdown=[&]{logs.push_back("detach");};
    return instance;
  };
  services.getFlightId=[&]{logs.push_back("sessionId");return std::string("session-1");};
  services.flightIdExists=[&](const std::string& id){logs.push_back("exists:"+id);return item.exists;};
  services.getCurrentFlightTitle=[&](const std::string& id){
    logs.push_back("titleLookup:"+id);return item.title;};
  services.dim=[](const std::string& value){return "dim:"+value;};
  services.wrapForMultiplexer=[](const std::string& value){return "wrap:"+value;};
  services.writeStdout=[&](const std::string& value){
    std::string escaped;for(char ch:value)escaped+=ch=='\n'?"\\n":std::string(1,ch);
    logs.push_back("write:"+escaped);
    if(item.writeError)throw std::runtime_error("closed");
  };
  services.setProcessTitle=[&](const std::string& value){logs.push_back("title:"+value);};
  services.sequences={"MOUSE","ALT","OTHER","KITTY","FOCUS","BRACKET",
    "CURSOR","PROGRESS","TAB","TITLE"};
  cleanup::TerminalLifecycle terminal(services);
  terminal.cleanupTerminalModes();terminal.printResumeHint();terminal.printResumeHint();
  return Json{{"name",item.name},{"logs",logs}};
}
}
int main(){
  const std::vector<Case> cases={{"non-tty"},
    {"active-title",true,true,true,true,true,false,false,false,false,"a\"b\\c"},
    {"unmount-failure",true,true,true,true,false,true},
    {"missing-session",true,true,false,false,false,false,true},
    {"windows",true,false,false,false,false,false,false,true},
    {"write-failure",true,true,true,false,false,false,false,false,true}};
  Json out=Json::array();for(const auto& item:cases)out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
