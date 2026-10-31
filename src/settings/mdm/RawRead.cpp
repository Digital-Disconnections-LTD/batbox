#include "settings/mdm/RawRead.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::settings::mdm {
using event_loop::Deferred;
namespace {
RawReadResult emptyRaw(){return {std::nullopt,std::nullopt,std::nullopt};}
}
RawReader::RawReader(event_loop::Loop& loop,RawReadServices services):
  loop_(loop),services_(std::move(services)){}
Deferred<RawReadResult> RawReader::fireRawRead(){
  if(!loop_.isOwnerThread())
    throw std::logic_error("MDM raw read requires owner loop");
  Deferred<RawReadResult> output(loop_);
  try{
    if(services_.platform==Platform::Linux){output.resolve(emptyRaw());return output;}
    if(!services_.execFile)
      throw std::invalid_argument("Missing MDM subprocess binding");
    if(services_.platform==Platform::MacOS){
      if(!services_.existsSync)
        throw std::invalid_argument("Missing MDM file-existence binding");
      const auto paths=getMacOSPlistPaths(services_);
      struct State {
        Deferred<RawReadResult> output;
        std::vector<std::optional<RawExec>> results;
        std::vector<PlistPath> paths;
        std::size_t pending=0;
        std::exception_ptr firstError;
      };
      auto state=std::make_shared<State>(State{output,
        std::vector<std::optional<RawExec>>(paths.size()),paths,0,{}});
      auto finish=[state](){
        if(state->pending)return;
        if(state->firstError){state->output.reject(state->firstError);return;}
        std::vector<PlistStdout> winner;
        for(std::size_t i=0;i<state->results.size();++i){
          const auto& result=state->results[i];
          if(result&&result->code==0&&!result->stdoutText.empty()){
            winner.push_back({result->stdoutText,state->paths[i].label});
            break;
          }
        }
        state->output.resolve({std::move(winner),std::nullopt,std::nullopt});
      };
      for(std::size_t i=0;i<paths.size();++i){
        bool exists=false;
        try{exists=services_.existsSync(paths[i].path);}
        catch(...){state->firstError=std::current_exception();continue;}
        if(!exists)continue;
        ++state->pending;
        try{
          services_.execFile(services_.names.plutilPath,
            {"-convert","json","-o","-","--",paths[i].path},5000)
            .then([state,finish,i](const RawExec* value,
                const std::exception_ptr& error){
              if(error&&!state->firstError)state->firstError=error;
              else if(value)state->results[i]=*value;
              --state->pending;finish();
            });
        }catch(...){if(!state->firstError)state->firstError=
            std::current_exception();--state->pending;}
      }
      finish();return output;
    }
    struct State {
      Deferred<RawReadResult> output;
      std::optional<RawExec> hklm,hkcu;
      std::exception_ptr firstError;
      int pending=2;
    };
    auto state=std::make_shared<State>(State{output,{},{},{},2});
    auto finish=[state](){
      if(state->pending)return;
      if(state->firstError){state->output.reject(state->firstError);return;}
      state->output.resolve({std::nullopt,
        state->hklm&&state->hklm->code==0?
          std::optional(state->hklm->stdoutText):std::nullopt,
        state->hkcu&&state->hkcu->code==0?
          std::optional(state->hkcu->stdoutText):std::nullopt});
    };
    const auto spawn=[&](const std::string& key,
        std::optional<RawExec>* slot){
      try{services_.execFile("reg",{"query",key,"/v",
          services_.names.windowsValue},5000)
        .then([state,finish,slot](const RawExec* value,
            const std::exception_ptr& error){
          if(error&&!state->firstError)state->firstError=error;
          else if(value)*slot=*value;
          --state->pending;finish();
        });
      }catch(...){if(!state->firstError)state->firstError=
          std::current_exception();--state->pending;}
    };
    spawn(services_.names.windowsHKLM,&state->hklm);
    spawn(services_.names.windowsHKCU,&state->hkcu);
    finish();
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void RawReader::startMdmRawRead(){
  if(startup_)return;
  startup_=fireRawRead();
}
std::optional<Deferred<RawReadResult>> RawReader::getMdmRawReadPromise()const{
  return startup_;
}
}
