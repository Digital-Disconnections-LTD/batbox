#pragma once

#include "interactive/background/FlightMission.hpp"

namespace batbox::interactive::background {
struct FlightBackgroundingServices {
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<FlightMissions> flightMissions;
  std::function<TurnMessagesRef(const std::string&,const Json&)> messagesForMission;
  std::function<std::shared_ptr<TurnAbort>(const std::string&,const Json&)>
    abortForMission;
  std::function<void(const TurnMessagesRef&)> setMessages;
  std::function<void(bool)> setIsLoading;
  std::function<void()> resetLoadingState,onBackgroundQuery;
  std::function<void(std::shared_ptr<TurnAbort>)> setAbortController;
};

class FlightBackgrounding:public std::enable_shared_from_this<FlightBackgrounding>{
public:
  FlightBackgrounding(event_loop::Loop& owner,FlightBackgroundingServices services);
  ~FlightBackgrounding();
  void start();
  void handleBackgroundFlight();
  void refresh();
private:
  event_loop::Loop& owner_;
  FlightBackgroundingServices services_;
  std::size_t lastSyncedMessagesLength_=0;
  bool refreshQueued_=false;
  std::function<void()> unsubscribe_;
  void scheduleRefresh();
  void clearForegroundId();
  void backgroundForegroundMission();
};
} // namespace batbox::interactive::background
