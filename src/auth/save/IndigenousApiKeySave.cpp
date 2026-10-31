#include "auth/save/IndigenousApiKeySave.hpp"
#include "locking/Async.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::save {
void bindIndigenousApiKeyCommands(ApiKeySaveServices& services,
    event_loop::Loop& loop,
    std::shared_ptr<storage::KeychainState> state,
    storage::MacKeychainOps operations){
  if(!state||!operations.executeSync)
    throw std::invalid_argument("Missing native API key command binding");
  services.keychainServiceName=[state](){return state->serviceName();};
  services.username=[state](){return state->username();};
  services.deleteKeychainEntry=[&loop,state,operations](
      const std::string& name){
    const auto user=state->services().userEnv().value_or(std::string{});
    return locking::runJob<storage::SecurityResult>(loop,
      [operations,user,name]{
        return operations.executeSync({"delete-generic-password","-a",
          user,"-s",name},std::nullopt);
      });
  };
  services.addInteractiveKeychainEntry=[&loop,operations](
      const std::string& command){
    return locking::runJob<storage::SecurityResult>(loop,
      [operations,command]{
        return operations.executeSync({"-i"},command);
      });
  };
}
}
