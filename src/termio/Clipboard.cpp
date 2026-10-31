#include "termio/Clipboard.hpp"
#include "termio/OscLink.hpp"
#include "runtime/Encoding.hpp"
#include "media/Base64.hpp"
namespace batbox::termio {
namespace {
bool present(const std::optional<std::string>& value){return value&&!value->empty();}
std::string tmuxPassthrough(const std::string& payload){
  std::string doubled;
  for(const auto c:payload){if(c=='\x1b')doubled+='\x1b';doubled+=c;}
  return "\x1bPtmux;"+doubled+"\x1b\\";
}
process::ExecOptions copyOptions(const std::string& text){
  process::ExecOptions options;options.input=text;options.useCwd=false;options.timeout=2000;return options;
}
}
struct ClipboardModule::Core {
  event_loop::Loop& loop;
  ClipboardServices services;
  std::optional<std::optional<std::string>> linuxCopy;
  Core(event_loop::Loop& loop,ClipboardServices services):loop(loop),services(std::move(services)){}
};
ClipboardModule::ClipboardModule(event_loop::Loop& loop,ClipboardServices services):
  core_(std::make_shared<Core>(loop,std::move(services))){}
std::string ClipboardModule::getClipboardPath()const{
  const auto& env=core_->services.environment;
  const auto indigenousAvailable=env.platform=="darwin"&&!present(env.getenv("SSH_CONNECTION"));
  if(indigenousAvailable)return "native";
  if(present(env.getenv("TMUX")))return "tmux-buffer";
  return "osc52";
}
std::string ClipboardModule::wrapForMultiplexer(const std::string& sequence)const{
  const auto& env=core_->services.environment;
  if(present(env.getenv("TMUX")))return tmuxPassthrough(sequence);
  if(present(env.getenv("STY")))return "\x1bP"+sequence+"\x1b\\";
  return sequence;
}
event_loop::Deferred<bool> ClipboardModule::tmuxLoadBuffer(const std::string& text){
  event_loop::Deferred<bool> output(core_->loop);
  try{
    const auto& env=core_->services.environment;
    if(!present(env.getenv("TMUX"))){output.resolve(false);return output;}
    const auto args=env.getenv("LC_TERMINAL")==std::optional<std::string>("iTerm2")?
      std::vector<std::string>{"load-buffer","-"}:std::vector<std::string>{"load-buffer","-w","-"};
    core_->services.execFileNoThrow("tmux",args,copyOptions(text)).then(
      [output](const process::ExecResult* result,const std::exception_ptr& error){
        if(error)output.reject(error);else output.resolve(result->code==0);
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void ClipboardModule::copyIndigenous(const std::string& text){
  const auto core=core_;const auto options=copyOptions(text);
  const auto& platform=core->services.environment.platform;
  if(platform=="darwin"){core->services.execFileNoThrow("pbcopy",{},options);return;}
  if(platform=="linux"){
    if(core->linuxCopy){
      if(!*core->linuxCopy)return;
      const auto& gadget=**core->linuxCopy;
      if(gadget=="wl-copy"){core->services.execFileNoThrow("wl-copy",{},options);return;}
      if(gadget=="xclip"){core->services.execFileNoThrow("xclip",{"-selection","clipboard"},options);return;}
      if(gadget=="xsel"){core->services.execFileNoThrow("xsel",{"--clipboard","--input"},options);return;}
    }
    core->services.execFileNoThrow("wl-copy",{},options).then([core,options](const process::ExecResult* result,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      if(result->code==0){core->linuxCopy=std::optional<std::string>("wl-copy");return;}
      core->services.execFileNoThrow("xclip",{"-selection","clipboard"},options).then([core,options](const process::ExecResult* result,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        if(result->code==0){core->linuxCopy=std::optional<std::string>("xclip");return;}
        core->services.execFileNoThrow("xsel",{"--clipboard","--input"},options).then([core](const process::ExecResult* result,const std::exception_ptr& error){
          if(error)std::rethrow_exception(error);
          core->linuxCopy=result->code==0?std::optional<std::string>("xsel"):std::optional<std::string>{};
        });
      });
    });
    return;
  }
  if(platform=="win32")core->services.execFileNoThrow("clip",{},options);
}
event_loop::Deferred<std::string> ClipboardModule::setClipboard(const std::string& text){
  event_loop::Deferred<std::string> output(core_->loop);
  try{
    const auto b64=media::indigenous::base64Encode(value::encodeUtf8(text));
    const auto raw=osc(Json::array({52,"c",b64}),core_->services.terminalName());
    if(!present(core_->services.environment.getenv("SSH_CONNECTION")))copyIndigenous(text);
    tmuxLoadBuffer(text).then([output,b64,raw](const bool* loaded,const std::exception_ptr& error){
      if(error)output.reject(error);
      else output.resolve(*loaded?tmuxPassthrough("\x1b]52;c;"+b64+"\x07"):raw);
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void ClipboardModule::resetLinuxCopyStash(){core_->linuxCopy.reset();}
}
