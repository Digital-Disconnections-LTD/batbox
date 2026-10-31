#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::storage {
struct Storage {
  std::string name;
  std::function<std::optional<Json>()> read;
  std::function<event_loop::Deferred<std::optional<Json>>()> readAsync;
  std::function<Json(const Json&)> update;
  std::function<bool()> remove;
};
struct PlaintextServices {
  std::function<std::string()> configHome;
  std::function<std::string(const std::string&)> readFileSync;
  std::function<event_loop::Deferred<std::string>(
    const std::string&)> readFileAsync;
  std::function<void(const std::string&)> mkdirSync;
  std::function<void(const std::string&,const std::string&)> writeFileSync;
  std::function<void(const std::string&,int)> chmodSync;
  std::function<void(const std::string&)> unlinkSync;
  std::string fileName=".batbox-credentials.json";
};
Storage makePlaintextStorage(event_loop::Loop& loop,
  PlaintextServices services);
Storage makeFallbackStorage(event_loop::Loop& loop,
  Storage primary,Storage secondary);
Storage selectStorage(event_loop::Loop& loop,const std::string& platform,
  Storage plaintext,std::optional<Storage> macOsKeychain);
PlaintextServices indigenousPlaintextServices(event_loop::Loop& loop,
  std::function<std::string()> configHome);
}
