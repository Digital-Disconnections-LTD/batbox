#include "auth/save/ApiKeySave.hpp"
#include "auth/ApiKey.hpp"
#include "runtime/Encoding.hpp"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace batbox::auth::save {
namespace {
std::string toHex(const std::string& value){
  const auto bytes=value::encodeUtf8(value);
  std::ostringstream output;
  output<<std::hex<<std::setfill('0');
  for(const auto byte:bytes)
    output<<std::setw(2)<<static_cast<int>(
      static_cast<unsigned char>(byte));
  return output.str();
}
Json property(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)?value[key]:Json(nullptr);
}
}
bool isValidApiKey(const std::string& key){
  if(key.empty())return false;
  for(const unsigned char byte:key){
    if(!((byte>='a'&&byte<='z')||(byte>='A'&&byte<='Z')||
        (byte>='0'&&byte<='9')||byte=='-'||byte=='_'))return false;
  }
  return true;
}
ApiKeySaver::ApiKeySaver(event_loop::Loop& loop,
    ApiKeySaveServices services):
  loop_(loop),services_(std::move(services)){
  if(!services_.platform||!services_.saveGlobalConfig||
      !services_.globalConfig||
      !services_.clearLegacyKeyMemo||!services_.clearLegacyPrefetch||
      !services_.logError)
    throw std::invalid_argument("Missing API key save binding");
}
event_loop::Deferred<event_loop::Unit>
ApiKeySaver::removeKeychainEntry(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  try{
    if(services_.platform()!="darwin"){
      output.resolve({});return output;
    }
    if(!services_.keychainServiceName||!services_.deleteKeychainEntry)
      throw std::invalid_argument("Missing keychain delete binding");
    const auto name=services_.keychainServiceName();
    services_.deleteKeychainEntry(name).then([output](
        const storage::SecurityResult* result,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      if(result->exitCode!=0){
        output.reject(std::make_exception_ptr(
          std::runtime_error("Failed to delete keychain entry")));
      }else output.resolve({});
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void ApiKeySaver::finishSave(const std::string& key,
    bool savedToKeychain){
  const auto normalized=normalizeApiKeyForConfig(key);
  services_.saveGlobalConfig([key,normalized,savedToKeychain](
      const Json& current){
    Json result=current.is_object()?current:Json::object();
    Json responses=property(current,"customApiKeyResponses");
    if(!responses.is_object())responses=Json::object();
    Json approved=property(responses,"approved");
    if(approved.is_null())approved=Json::array();
    bool found=false;
    if(approved.is_array())for(const auto& item:approved){
      if(item==normalized){found=true;break;}
    }
    if(!found)approved.push_back(normalized);
    responses["approved"]=approved;
    const auto rejected=property(responses,"rejected");
    responses["rejected"]=rejected.is_null()
      ?Json::array():rejected;
    result["customApiKeyResponses"]=responses;
    if(!savedToKeychain)result["primaryApiKey"]=key;
    return result;
  });
  services_.clearLegacyKeyMemo();
  services_.clearLegacyPrefetch();
}
event_loop::Deferred<event_loop::Unit>
ApiKeySaver::save(const std::string& key){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  if(!isValidApiKey(key)){
    output.reject(std::make_exception_ptr(std::runtime_error(
      "Invalid API key format. API key must contain only alphanumeric "
      "characters, dashes, and underscores.")));
    return output;
  }
  removeKeychainEntry().then([this,key,output](
      const event_loop::Unit*,const std::exception_ptr& removalError){
    if(removalError){
      try{services_.logError(removalError);}
      catch(...){output.reject(std::current_exception());return;}
    }
    if(services_.platform()!="darwin"){
      try{finishSave(key,false);output.resolve({});}
      catch(...){output.reject(std::current_exception());}
      return;
    }
    try{
      if(!services_.keychainServiceName||!services_.username||
          !services_.addInteractiveKeychainEntry)
        throw std::invalid_argument("Missing keychain add binding");
      const auto name=services_.keychainServiceName();
      const auto username=services_.username();
      const auto command="add-generic-password -U -a \""+username+
        "\" -s \""+name+"\" -X \""+toHex(key)+"\"\n";
      services_.addInteractiveKeychainEntry(command).then(
        [this,key,output](const storage::SecurityResult*,
            const std::exception_ptr& addError){
        try{
          if(addError)services_.logError(addError);
          finishSave(key,!addError);
          output.resolve({});
        }catch(...){output.reject(std::current_exception());}
      });
    }catch(...){
      try{
        services_.logError(std::current_exception());
        finishSave(key,false);output.resolve({});
      }catch(...){output.reject(std::current_exception());}
    }
  });
  return output;
}
void ApiKeySaver::finishRemove(){
  services_.saveGlobalConfig([](const Json& current){
    Json result=current.is_object()?current:Json::object();
    result.erase("primaryApiKey");return result;
  });
  services_.clearLegacyKeyMemo();
  services_.clearLegacyPrefetch();
}
event_loop::Deferred<event_loop::Unit> ApiKeySaver::remove(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  removeKeychainEntry().then([this,output](
      const event_loop::Unit*,const std::exception_ptr& removalError){
    try{
      if(removalError)services_.logError(removalError);
      finishRemove();output.resolve({});
    }catch(...){output.reject(std::current_exception());}
  });
  return output;
}
bool ApiKeySaver::isApproved(const std::string& key)const{
  const auto config=services_.globalConfig();
  const auto responses=property(config,"customApiKeyResponses");
  const auto approved=property(responses,"approved");
  if(!approved.is_array())return false;
  const auto normalized=normalizeApiKeyForConfig(key);
  for(const auto& value:approved)if(value==normalized)return true;
  return false;
}
}
