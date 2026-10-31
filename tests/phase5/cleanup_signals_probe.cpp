#include "cleanup/Signals.hpp"
#include "runtime/JsonValue.hpp"
#include <csignal>
#include <iostream>
#include <map>
#include <vector>

using namespace batbox;
namespace {
struct Case {std::string name;bool tty=false,print=false,windows=false;};
Json run(const Case& item){
  Json logs=Json::array();std::map<int,std::function<void()>> signals;
  std::function<void(cleanup::DiagnosticError)> uncaught;
  std::function<void(cleanup::RejectionReason)> rejection;
  std::function<void()> interval;
  bool scroll=false,writable=true,readable=true;
  cleanup::SignalServices services;
  services.pinExitHook=[&]{logs.push_back("pinExit");};
  services.onSignal=[&](int signal,std::function<void()> callback){
    const auto name=signal==SIGINT?"SIGINT":signal==SIGTERM?"SIGTERM":"SIGHUP";
    logs.push_back(std::string("on:")+name);signals[signal]=std::move(callback);
  };
  services.onUncaughtException=[&](std::function<void(cleanup::DiagnosticError)> callback){
    logs.push_back("on:uncaughtException");uncaught=std::move(callback);
  };
  services.onUnhandledRejection=[&](std::function<void(cleanup::RejectionReason)> callback){
    logs.push_back("on:unhandledRejection");rejection=std::move(callback);
  };
  services.setInterval=[&](double delay,std::function<void()> callback){
    logs.push_back("interval:"+value::numberToString(delay));interval=std::move(callback);
    return cleanup::ShutdownTimer{[&]{logs.push_back("clearInterval");},
      [&]{logs.push_back("unref:"+value::numberToString(delay));}};
  };
  services.isPrintMode=[&]{return item.print;};
  services.isWindows=[&]{return item.windows;};
  services.stdinIsTTY=[&]{return item.tty;};
  services.isScrollDraining=[&]{return scroll;};
  services.stdoutWritable=[&]{return writable;};
  services.stdinReadable=[&]{return readable;};
  services.localDiagnostic=[&](const std::string& level,const std::string& event,Json data){
    logs.push_back(Json{{"level",level},{"event",event},{"data",std::move(data)}});
  };
  services.gracefulShutdown=[&](int code){logs.push_back("shutdown:"+std::to_string(code));};
  cleanup::SignalLifecycle lifecycle(services);
  lifecycle.setupGracefulShutdown();lifecycle.setupGracefulShutdown();
  if(signals.count(SIGINT))signals.at(SIGINT)();
  if(signals.count(SIGTERM))signals.at(SIGTERM)();
  if(signals.count(SIGHUP))signals.at(SIGHUP)();
  if(interval){scroll=true;interval();scroll=false;writable=false;interval();}
  uncaught({"Error","oops",{}});
  rejection({cleanup::DiagnosticError{"Error","bad rejection","stack text"},""});
  rejection({{},"string reason"});
  return Json{{"name",item.name},{"logs",logs}};
}
}
int main(){
  const std::vector<Case> cases={{"posix-tty",true,false,false},
    {"print-mode",false,true,false},{"windows-tty",true,false,true}};
  Json out=Json::array();for(const auto& item:cases)out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
