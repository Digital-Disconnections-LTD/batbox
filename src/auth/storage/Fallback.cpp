#include "auth/storage/Storage.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::storage {
namespace {
bool success(const Json& value){
  return value.is_object()&&value.contains("success")&&
    value::truthy(value["success"]);
}
}
Storage makeFallbackStorage(event_loop::Loop& loop,
    Storage primary,Storage secondary){
  if(!primary.read||!primary.readAsync||!primary.update||!primary.remove||
      !secondary.read||!secondary.readAsync||!secondary.update||
      !secondary.remove)
    throw std::invalid_argument("Missing fallback storage operation");
  Storage result;
  result.name=primary.name+"-with-"+secondary.name+"-fallback";
  result.read=[primary,secondary]()->std::optional<Json>{
    const auto first=primary.read();
    if(first)return first;
    const auto second=secondary.read();
    return second&&value::truthy(*second)
      ?second:std::optional<Json>(Json::object());
  };
  result.readAsync=[&loop,primary,secondary](){
    event_loop::Deferred<std::optional<Json>> output(loop);
    try{primary.readAsync().then([output,secondary](
        const std::optional<Json>* first,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      if(*first){output.resolve(*first);return;}
      try{secondary.readAsync().then([output](
          const std::optional<Json>* second,
          const std::exception_ptr& secondaryError){
        if(secondaryError){output.reject(secondaryError);return;}
        output.resolve(*second&&value::truthy(**second)
          ?*second:std::optional<Json>(Json::object()));
      });}catch(...){output.reject(std::current_exception());}
    });}catch(...){output.reject(std::current_exception());}
    return output;
  };
  result.update=[primary,secondary](const Json& data){
    const auto before=primary.read();
    const auto outcome=primary.update(data);
    if(success(outcome)){
      if(!before)secondary.remove();
      return outcome;
    }
    const auto fallback=secondary.update(data);
    if(success(fallback)){
      if(before)primary.remove();
      Json value={{"success",true}};
      if(fallback.is_object()&&fallback.contains("warning"))
        value["warning"]=fallback["warning"];
      return value;
    }
    return Json{{"success",false}};
  };
  result.remove=[primary,secondary](){
    const bool first=primary.remove();
    const bool second=secondary.remove();
    return first||second;
  };
  return result;
}
Storage selectStorage(event_loop::Loop& loop,const std::string& platform,
    Storage plaintext,std::optional<Storage> macOsKeychain){
  if(platform=="darwin"){
    if(!macOsKeychain)
      throw std::invalid_argument("Missing macOS keychain provider");
    return makeFallbackStorage(loop,std::move(*macOsKeychain),
      std::move(plaintext));
  }
  return plaintext;
}
}
