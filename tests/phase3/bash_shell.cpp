#include <cstdlib>
#include "shell/ManagedShell.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace batbox::shell;
int main(){
 char dir[]="/tmp/batbox-shell-test-XXXXXX";if(!::mkdtemp(dir))return 1;
 std::filesystem::path root=std::filesystem::canonical(dir);std::filesystem::create_directory(root/"sub");
 std::atomic<int> next{0};
 FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
 config.outputDir=(root/"jobs").string();config.maxInlineOutputBytes=100;
 config.generateMissionId=[&]{return "test"+std::to_string(++next);};
 config.splitCommand=[](const std::string& command){
   return std::vector<std::string>{command.rfind("sleep ",0)==0?"sleep":command};};
 ManagedShell shell(config);
 auto ok=shell.run({.command="printf 'hello 🌍\\n'; printf 'err\\n' >&2"});
 if(ok.code!=0||ok.stdoutText.find("hello 🌍")==std::string::npos||ok.stdoutText.find("err")==std::string::npos){
   std::cerr<<"basic shell failed: "<<ok.toJson().dump()<<'\n';return 1;}
 auto moved=shell.run({.command="cd sub && pwd"});
 if(moved.code!=0||shell.cwd()!=(root/"sub").string()){
   std::cerr<<"cwd update failed: "<<moved.toJson().dump()<<'\n';return 1;}
 auto failed=shell.run({.command="cd ..; false"});
 if(failed.code!=1||shell.cwd()!=(root/"sub").string()){
   std::cerr<<"failed command changed cwd\n";return 1;}
 auto large=shell.run({.command="printf '%0200d' 0"});
 if(!large.outputFilePath||large.outputFileSize.value_or(0)<200||large.stdoutText.size()!=100){
   std::cerr<<"large output persistence failed: "<<large.toJson().dump()<<'\n';return 1;}
 auto timed=shell.run({.command="sleep 2",.timeoutMs=100});
 if(timed.code!=143){std::cerr<<"timeout failed: "<<timed.toJson().dump()<<'\n';return 1;}
 auto bg=shell.run({.command="sleep 0.3; echo background",.runInBackground=true});
 if(!bg.backgroundMissionId){std::cerr<<"background ID missing\n";return 1;}
 auto done=shell.wait(*bg.backgroundMissionId,2000);
 if(done.code!=0||done.stdoutText.find("background")==std::string::npos){
   std::cerr<<"background wait failed: "<<done.toJson().dump()<<'\n';return 1;}
 if(!shell.cleanup(*bg.backgroundMissionId)){std::cerr<<"background cleanup failed\n";return 1;}
 auto stopped=shell.run({.command="sleep 30",.runInBackground=true});
 auto killed=shell.stop(*stopped.backgroundMissionId);
 if(killed.code!=137||!killed.interrupted){std::cerr<<"stop failed\n";return 1;}
 shell.cleanup(*stopped.backgroundMissionId);
 RunOptions foreground;foreground.command="sleep 30";
 const auto userId="test"+std::to_string(next.load()+1);
 auto userFuture=std::async(std::launch::async,[&]{return shell.run(foreground);});
 bool movedToBackground=false;
 for(int attempt=0;attempt<100&&!movedToBackground;attempt++){
   movedToBackground=shell.background(userId);
   if(!movedToBackground)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 if(!movedToBackground){std::cerr<<"foreground backgrounding failed\n";return 1;}
 auto userMoved=userFuture.get();
 if(!userMoved.backgroundMissionId||!userMoved.backgroundedByUser){std::cerr<<"user background flag missing\n";return 1;}
 shell.stop(userId);shell.cleanup(userId);
 std::stop_source stop;
 RunOptions opt;opt.command="sleep 30";opt.signal=stop.get_token();
 auto future=std::async(std::launch::async,[&]{return shell.run(opt);});
 std::this_thread::sleep_for(std::chrono::milliseconds(100));stop.request_stop();
 auto aborted=future.get();
 if(aborted.code!=137||!aborted.interrupted){std::cerr<<"cancellation failed: "<<aborted.toJson().dump()<<'\n';return 1;}
 std::stop_source interrupt;
 RunOptions interruptedOptions;interruptedOptions.command="sleep 0.3; echo survived";
 interruptedOptions.signal=interrupt.get_token();
 interruptedOptions.currentAbortReason=[]{return std::string("interrupt");};
 const auto interruptId="test"+std::to_string(next.load()+1);
 auto interruptedFuture=std::async(std::launch::async,[&]{return shell.run(interruptedOptions);});
 std::this_thread::sleep_for(std::chrono::milliseconds(80));
 interrupt.request_stop();
 bool interruptBackgrounded=false;
 for(int attempt=0;attempt<100&&!interruptBackgrounded;attempt++){
   interruptBackgrounded=shell.background(interruptId);
   if(!interruptBackgrounded)std::this_thread::sleep_for(std::chrono::milliseconds(5));
 }
 if(!interruptBackgrounded){std::cerr<<"interrupt process could not background\n";return 1;}
 const auto interruptedReturn=interruptedFuture.get();
 if(!interruptedReturn.backgroundMissionId){std::cerr<<"interrupt lost background ID\n";return 1;}
 const auto survived=shell.wait(interruptId,2000);
 if(survived.code!=0||survived.stdoutText.find("survived")==std::string::npos){
   std::cerr<<"interrupt killed background process\n";return 1;}
 shell.cleanup(interruptId);
 // A throwing splitter must fail before the child exists.
 auto throwConfig=config;
 throwConfig.splitCommand=[](const std::string&)->std::vector<std::string>{
   throw std::runtime_error("split failure");};
 ManagedShell throwShell(throwConfig);
 const auto before=next.load();
 bool splitThrew=false;
 try{throwShell.run({.command="touch "+(root/"should-not-exist").string()});}
 catch(const std::runtime_error& error){splitThrew=std::string(error.what())=="split failure";}
 if(!splitThrew||std::filesystem::exists(root/"should-not-exist")||
    std::filesystem::exists(throwShell.outputPath("test"+std::to_string(before+1)))){
   std::cerr<<"throwing splitter spawned a child\n";return 1;}
 // An arbitrary completion sink exception must be retained and diagnosed.
 std::atomic<int> diagnosed{0};
 auto callbackConfig=config;
 callbackConfig.onBackgroundComplete=[](const std::string&,const ExecResult&){
   throw std::runtime_error("completion sink failure");};
 callbackConfig.onBackgroundError=[&](const std::string&,std::exception_ptr error){
   try{std::rethrow_exception(error);}catch(const std::runtime_error& e){
     if(std::string(e.what())=="completion sink failure")++diagnosed;}};
 ManagedShell callbackShell(callbackConfig);
 auto callbackJob=callbackShell.run({.command="echo done",.runInBackground=true});
 if(!callbackJob.backgroundMissionId){std::cerr<<"completion test job not backgrounded\n";return 1;}
 callbackShell.wait(*callbackJob.backgroundMissionId,2000);
 std::exception_ptr callbackError;
 for(int i=0;i<100&&!callbackError;++i){
   callbackError=callbackShell.backgroundCallbackFailure(*callbackJob.backgroundMissionId);
   if(!callbackError)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 bool originalException=false;
 try{if(callbackError)std::rethrow_exception(callbackError);}
 catch(const std::runtime_error& e){originalException=std::string(e.what())=="completion sink failure";}
 if(!originalException||diagnosed.load()!=1){std::cerr<<"completion exception lost\n";return 1;}
 callbackShell.cleanup(*callbackJob.backgroundMissionId);
 std::filesystem::remove_all(root);
 std::cout<<"PASS native shell basic/cwd/output/background/stop/cancel/interrupt/timeout/throw safety\n";
}
