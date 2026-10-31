#pragma once
#include "auth/storage/MacKeychain.hpp"
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <string>

namespace batbox::auth::save {
struct ApiKeySaveServices {
  std::function<std::string()> platform,keychainServiceName,username;
  std::function<event_loop::Deferred<storage::SecurityResult>(
    const std::string&)> deleteKeychainEntry;
  std::function<event_loop::Deferred<storage::SecurityResult>(
    const std::string&)> addInteractiveKeychainEntry;
  std::function<void(const std::function<Json(const Json&)>&)>
    saveGlobalConfig;
  std::function<Json()> globalConfig;
  std::function<void()> clearLegacyKeyMemo,clearLegacyPrefetch;
  std::function<void(const std::exception_ptr&)> logError;
};
class ApiKeySaver {
public:
  ApiKeySaver(event_loop::Loop& loop,ApiKeySaveServices services);
  event_loop::Deferred<event_loop::Unit> save(const std::string& key);
  event_loop::Deferred<event_loop::Unit> remove();
  bool isApproved(const std::string& key)const;
private:
  event_loop::Loop& loop_;
  ApiKeySaveServices services_;
  event_loop::Deferred<event_loop::Unit> removeKeychainEntry();
  void finishSave(const std::string& key,bool savedToKeychain);
  void finishRemove();
};
bool isValidApiKey(const std::string& key);
}
