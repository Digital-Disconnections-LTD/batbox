#include "auth/save/SaveTokens.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
std::string message(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown error";}
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    Json state=test.value("storage",Json(nullptr));
    auth::save::Services services;
    services.getSecureStorage=[&](){
      trace.push_back("storage");
      if(test.value("storageError",false))
        throw std::runtime_error("storage failed");
      auth::save::SecureStorage storage;
      storage.read=[&]()->std::optional<Json>{
        trace.push_back("read");
        if(test.value("readError",false))
          throw std::runtime_error("read failed");
        return state.is_null()?std::nullopt:std::optional(state);
      };
      storage.update=[&](const Json& data){
        trace.push_back(Json::array({"update",data}));
        if(test.value("updateError",false))
          throw std::runtime_error("update failed");
        state=data;
        return test.value("updateStatus",Json{{"success",true}});
      };
      return storage;
    };
    services.clearTokenStash=[&](){
      trace.push_back("clearMemo");
      if(test.value("clearMemoError",false))
        throw std::runtime_error("clear memo failed");
    };
    services.clearBetasStashes=[&](){
      trace.push_back("clearBetas");
      if(test.value("clearBetasError",false))
        throw std::runtime_error("clear betas failed");
    };
    services.clearGadgetSchemaStash=[&](){
      trace.push_back("clearSchema");
      if(test.value("clearSchemaError",false))
        throw std::runtime_error("clear schema failed");
    };
    services.logError=[&](const std::exception_ptr& error){
      trace.push_back(Json::array({"error",message(error)}));
    };
    Json value;
    try{value=auth::save::saveOAuthTokensIfNeeded(
        test.value("tokens",Json::object()),services);}
    catch(const std::exception& error){value=Json{{"error",error.what()}};}
    std::cout<<value::stringify(Json{{"value",value},{"trace",trace},
      {"state",state}})<<'\n';
  }
}
