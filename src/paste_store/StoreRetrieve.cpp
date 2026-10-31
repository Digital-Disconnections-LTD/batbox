#include "paste_store/PasteStore.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::paste_store {
namespace {
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("Paste store import is required");
}
struct StoreRun {
  StoreRun(std::string key,std::string text,Services providers):
    result(*providers.loop),hash(std::move(key)),content(std::move(text)),
    services(std::move(providers)){}
  event_loop::Deferred<event_loop::Unit> result;
  std::string hash,content,path;
  Services services;
};
void storeFailed(const std::shared_ptr<StoreRun>& run,std::exception_ptr error){
  try{
    required(run->services.logForDebugging);
    required(run->services.errorToString);
    run->services.logForDebugging("Failed to store paste: "+
      run->services.errorToString(error));
    run->result.resolve({});
  }catch(...){run->result.reject(std::current_exception());}
}
struct RetrieveRun {
  RetrieveRun(std::string key,Services providers):
    result(*providers.loop),hash(std::move(key)),services(std::move(providers)){}
  event_loop::Deferred<std::optional<std::string>> result;
  std::string hash;
  Services services;
};
void retrieveFailed(const std::shared_ptr<RetrieveRun>& run,
    std::exception_ptr error){
  try{
    required(run->services.isENOENT);
    if(!run->services.isENOENT(error)){
      required(run->services.logForDebugging);
      required(run->services.errorToString);
      run->services.logForDebugging("Failed to retrieve paste "+run->hash+
        ": "+run->services.errorToString(error));
    }
    run->result.resolve(std::nullopt);
  }catch(...){run->result.reject(std::current_exception());}
}
}
event_loop::Deferred<event_loop::Unit> storePastedText(
    const std::string& hash,const std::string& content,const Services& services){
  if(!services.loop)throw std::invalid_argument("Paste store owner loop required");
  auto run=std::make_shared<StoreRun>(hash,content,services);
  try{
    const auto dir=getPasteStoreDir(run->services);
    required(run->services.mkdir);
    auto mkdir=run->services.mkdir(dir,Json::object({{"recursive",true}}));
    mkdir.then([run](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){storeFailed(run,error);return;}
      try{
        run->path=getPastePath(run->services,run->hash);
        required(run->services.writeFile);
        auto write=run->services.writeFile(run->path,run->content,
          Json::object({{"encoding","utf8"},{"mode",0600}}));
        write.then([run](const event_loop::Unit*,const std::exception_ptr& failure){
          if(failure){storeFailed(run,failure);return;}
          try{
            required(run->services.logForDebugging);
            run->services.logForDebugging("Stored paste "+run->hash+
              " to "+run->path);
            run->result.resolve({});
          }catch(...){storeFailed(run,std::current_exception());}
        });
      }catch(...){storeFailed(run,std::current_exception());}
    });
  }catch(...){storeFailed(run,std::current_exception());}
  return run->result;
}
event_loop::Deferred<std::optional<std::string>> retrievePastedText(
    const std::string& hash,const Services& services){
  if(!services.loop)throw std::invalid_argument("Paste store owner loop required");
  auto run=std::make_shared<RetrieveRun>(hash,services);
  try{
    const auto path=getPastePath(run->services,run->hash);
    required(run->services.readFile);
    auto read=run->services.readFile(path,Json::object({{"encoding","utf8"}}));
    read.then([run](const std::string* text,const std::exception_ptr& error){
      if(error)retrieveFailed(run,error);
      else run->result.resolve(*text);
    });
  }catch(...){retrieveFailed(run,std::current_exception());}
  return run->result;
}
}
