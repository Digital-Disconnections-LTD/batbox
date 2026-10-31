#pragma once
#include "settings/mdm/Parse.hpp"
#include <functional>
#include <optional>

namespace batbox::settings::mdm {
struct StoreServices {
  std::function<std::optional<event_loop::Deferred<RawReadResult>>()> startupRaw;
  std::function<event_loop::Deferred<RawReadResult>()> freshRaw;
  std::function<void(const std::string&)> profileCheckpoint;
  std::function<double()> nowMs;
  std::function<void(const std::string&)> debug;
  std::function<void(const std::string&,const std::string&,const Json&)>
    localDiagnostics;
};
class Store {
public:
  Store(event_loop::Loop& loop,ParseServices parser,StoreServices services);
  void startMdmSettingsLoad();
  event_loop::Deferred<event_loop::Unit> ensureMdmSettingsLoaded();
  MdmResult getMdmSettings() const;
  MdmResult getHkcuSettings() const;
  void clearMdmSettingsStash();
  void setMdmSettingsStash(MdmResult mdm,MdmResult hkcu);
  event_loop::Deferred<ParsedRaw> refreshMdmSettings();
  std::optional<event_loop::Deferred<event_loop::Unit>> loadPromise()const;
private:
  event_loop::Loop& loop_;
  ParseServices parser_;
  StoreServices services_;
  std::optional<MdmResult> mdmStash_,hkcuStash_;
  std::optional<event_loop::Deferred<event_loop::Unit>> loadPromise_;
};
}
