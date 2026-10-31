#include "interactive/repl_runtime/Runtime.hpp"
#include <stdexcept>

namespace batbox::interactive::repl_runtime {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("REPL init binding required: ")+name);
}
}
event_loop::Deferred<event_loop::Unit> Runtime::onInit(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("REPL init owner required");
  event_loop::Deferred<event_loop::Unit> result(services_.owner);
  try{
    need(services_.reverifyApiKey,"reverifyApiKey");
    auto verification=services_.reverifyApiKey();
    if(!services_.owner.dispatcher().sameLoop(verification.dispatcher()))
      throw std::invalid_argument("API verification Promise crossed owner loop");
    auto weak=weak_from_this();
    verification.then([weak](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(auto self=weak.lock())self->reportDetached(error);
    });
    need(services_.getMemoryFiles,"getMemoryFiles");
    auto pending=services_.getMemoryFiles();
    if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Memory-file Promise crossed owner loop");
    auto self=shared_from_this();
    pending.then([self,result](const std::vector<MemoryFile>* files,
        const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      try{
        if(!files)throw std::logic_error("Memory files missing");
        need(self->services_.logForDebugging,"logForDebugging");
        if(files->empty()){
          self->services_.logForDebugging(
            "No Batbox memory/rules files found");
        }else{
          std::string list;
          for(const auto& file:*files){
            if(!list.empty())list+='\n';
            list+="  ["+file.type+"] "+file.path+" ("+
              std::to_string(value::stringLength(file.content))+" chars)";
            if(file.parent&&!file.parent->empty())
              list+=" (included by "+*file.parent+")";
          }
          self->services_.logForDebugging("Loaded "+
            std::to_string(files->size())+
            " Batbox memory/rules files:\n"+list);
        }
        need(self->services_.nowMs,"nowMs");
        for(const auto& file:*files){
          const auto content=file.contentDiffersFromDisk?
            file.rawContent.value_or(file.content):file.content;
          self->services_.readFileState->set(file.path,
            file_state_stash::FileState(content,self->services_.nowMs(),
              file.contentDiffersFromDisk,{},{},true));
        }
        result.resolve({});
      }catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::interactive::repl_runtime
