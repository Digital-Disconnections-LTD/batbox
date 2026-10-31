#include "auth/storage/MacKeychain.hpp"
#include "runtime/Encoding.hpp"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace batbox::auth::storage {
namespace {
constexpr std::size_t stdinLineLimit=4096-64;
std::string toHex(const std::string& text){
  const auto bytes=value::encodeUtf8(text);
  std::ostringstream output;
  output<<std::hex<<std::setfill('0');
  for(const auto byte:bytes)
    output<<std::setw(2)<<static_cast<int>(
      static_cast<unsigned char>(byte));
  return output.str();
}
std::optional<Json> parsed(const std::string& text){
  auto value=value::parse(text);
  return value.is_null()?std::nullopt:std::optional<Json>(std::move(value));
}
std::string credentialService(const KeychainState& state){
  return state.serviceName(state.services().credentialServiceSuffix);
}
}
Storage makeMacKeychainStorage(event_loop::Loop& loop,
    std::shared_ptr<KeychainState> state,MacKeychainOps operations){
  if(!state||!operations.findSync||!operations.findAsync||
      !operations.executeSync||!operations.deleteSync||!operations.debug)
    throw std::invalid_argument("Missing macOS keychain binding");
  Storage result;
  result.name="keychain";
  result.read=[state,operations]()->std::optional<Json>{
    const auto previous=state->data;
    if(state->services().nowMs()-state->stashedAt<
        KeychainState::stashTtlMs)return previous;
    try{
      const auto service=credentialService(*state);
      const auto username=state->username();
      const auto text=operations.findSync(username,service);
      if(!text.empty()){
        const auto data=parsed(text);
        state->data=data;state->stashedAt=state->services().nowMs();
        return data;
      }
    }catch(...){}
    if(previous){
      operations.debug("[keychain] read failed; serving stale cache","warn");
      state->data=previous;
      state->stashedAt=state->services().nowMs();
      return previous;
    }
    state->data.reset();
    state->stashedAt=state->services().nowMs();
    return std::nullopt;
  };
  result.readAsync=[&loop,state,operations](){
    event_loop::Deferred<std::optional<Json>> output(loop);
    const auto previous=state->data;
    try{
      if(state->services().nowMs()-state->stashedAt<
          KeychainState::stashTtlMs){output.resolve(previous);return output;}
      if(state->readInFlight)return *state->readInFlight;
      const auto generation=state->generation;
      state->readInFlight=output;
      const auto finish=[state,operations,previous,generation,output](
          const std::optional<Json>& data){
        if(generation==state->generation){
          if(!data&&previous)
            operations.debug(
              "[keychain] readAsync failed; serving stale cache","warn");
          const auto next=data?data:previous;
          state->data=next;
          state->stashedAt=state->services().nowMs();
          state->readInFlight.reset();
          output.resolve(next);
        }else output.resolve(data);
      };
      try{
        const auto service=credentialService(*state);
        const auto username=state->username();
        operations.findAsync(username,service).then([finish](
            const SecurityResult* response,const std::exception_ptr& error){
          if(error||response->exitCode!=0||response->stdoutText.empty()){
            finish(std::nullopt);return;
          }
          try{finish(parsed(value::trim(response->stdoutText)));}
          catch(...){finish(std::nullopt);}
        });
      }catch(...){finish(std::nullopt);}
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  result.update=[state,operations](const Json& data){
    state->clear();
    try{
      const auto service=credentialService(*state);
      const auto username=state->username();
      const auto json=value::stringify(data);
      const auto hex=toHex(json);
      const auto command="add-generic-password -U -a \""+username+
        "\" -s \""+service+"\" -X \""+hex+"\"\n";
      SecurityResult response;
      if(value::utf16CodeUnits(command).size()<=stdinLineLimit){
        response=operations.executeSync({"-i"},command);
      }else{
        operations.debug("Keychain payload ("+
          std::to_string(value::utf16CodeUnits(json).size())+
          "B JSON) exceeds security -i stdin limit; using argv","warn");
        response=operations.executeSync({"add-generic-password","-U",
          "-a",username,"-s",service,"-X",hex},std::nullopt);
      }
      if(response.exitCode!=0)return Json{{"success",false}};
      state->data=data;state->stashedAt=state->services().nowMs();
      return Json{{"success",true}};
    }catch(...){return Json{{"success",false}};}
  };
  result.remove=[state,operations](){
    state->clear();
    try{
      const auto service=credentialService(*state);
      const auto username=state->username();
      operations.deleteSync(username,service);
      return true;
    }catch(...){return false;}
  };
  return result;
}
}
