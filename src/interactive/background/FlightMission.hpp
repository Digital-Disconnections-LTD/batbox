#pragma once

#include "bat_context/BatContext.hpp"
#include "app_state/State.hpp"
#include "interactive/background/BackgroundQuery.hpp"
#include <array>
#include <map>

namespace batbox::interactive::background {
struct FlightMissionServices {
  std::shared_ptr<app_state::AppStateStore> store;
  std::function<std::array<unsigned char,8>()> randomBytes8;
  std::function<std::int64_t()> nowMillis;
  std::function<std::string(const std::string&)> getMissionOutputPath,
    getBatTranscriptPath;
  std::function<void(const std::string&,const std::string&)>
    initMissionOutputAsSymlink;
  std::function<std::function<void()>(const std::function<void()>&)>
    registerCleanup;
  std::function<void(const Json&)> enqueueSdkEvent,enqueuePendingNotification;
  std::function<void(const std::string&,const std::string&,
    const std::optional<std::string>&,const std::string&)>
    emitMissionTerminatedSdk;
  std::function<void(const std::string&)> evictMissionOutput,logForDebugging;
  std::function<void(std::exception_ptr)> logError;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const TurnMessagesRef&,const std::string&,
    const std::optional<std::string>&)> recordSidechainTranscript;
  std::function<query::AsyncQueryCursor(query::Params,const CanUseGadget&)> makeQuery;
};

struct FlightMission {
  std::string id;
  std::shared_ptr<TurnAbort> abort;
  TurnMessagesRef messages;
  std::shared_ptr<poetryEntries::MainThreadPoetryBat> selectedBat;
  std::function<void()> unregisterCleanup;
};
using FlightMissionRef=std::shared_ptr<FlightMission>;

class FlightMissions:public std::enable_shared_from_this<FlightMissions> {
public:
  explicit FlightMissions(event_loop::Loop& owner,FlightMissionServices services);
  FlightMissionRef registerMainFlightMission(const std::string& description,
    const Json& batDefinition=Json(Json::value_t::discarded),
    std::shared_ptr<TurnAbort> existingAbort={},
    std::shared_ptr<poetryEntries::MainThreadPoetryBat> selectedBat={});
  void completeMainFlightMission(const std::string& id,bool success);
  void enqueueMainFlightNotification(const std::string& id,
    const std::string& description,const std::string& status,
    const std::optional<std::string>& gadgetUseId={});
  TurnMessagesRef foregroundMainFlightMission(const std::string& id);
  std::string startBackgroundFlight(StartRequest request);
  FlightMissionRef find(const std::string& id)const;
  static bool isMainFlightMission(const Json& value);
private:
  event_loop::Loop& owner_;
  std::shared_ptr<FlightMissionServices> services_;
  std::map<std::string,FlightMissionRef> sidecars_;
  std::string generateId()const;
  void updateMission(const std::string& id,
    const std::function<std::optional<Json>(const Json&)>& update);
  void setMission(const std::string& id,const Json& mission);
  void removeMission(const std::string& id);
  void refreshMessageView(const std::string& id,const TurnMessagesRef& messages);
  friend struct FlightRun;
};

} // namespace batbox::interactive::background
