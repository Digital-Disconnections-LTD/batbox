#pragma once
#include "settings/persistence/InternalWrites.hpp"
#include "settings/persistence/WatchTargets.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace batbox::settings::persistence {
using CancelTimer=std::function<void()>;
struct ChangeEventServices {
  WatchPathServices paths;
  InternalWrites* internalWrites=nullptr;
  std::function<void(const std::string&)> debug;
  // Deliver completion on the event-loop owner strand.
  std::function<void(const std::string&,const std::string&,
    std::function<void(const std::vector<bool>&)>)> executeConfigChangeHooks;
  std::function<CancelTimer(double,std::function<void()>)> setTimeout;
  std::function<void()> resetSettingsStash;
  double deletionGraceMs=1700;
};

class ChangeEvents {
public:
  using Listener=std::function<void(Source)>;
  explicit ChangeEvents(ChangeEventServices services);
  std::function<void()> subscribe(Listener listener);
  void handleChange(const std::string& path);
  void handleAdd(const std::string& path);
  void handleDelete(const std::string& path);
  void notifyChange(Source source);
  void setDeletionGrace(double milliseconds);
  void clearPendingDeletions();
  void clearListeners();
  void dispose();
private:
  struct ListenerEntry { Listener callback; bool active=true; };
  ChangeEventServices services_;
  std::map<std::string,CancelTimer> pendingDeletions_;
  std::vector<std::shared_ptr<ListenerEntry>> listeners_;
  void cancelDeletion(const std::string& path,const std::string& reason);
  void runHooks(Source source,const std::string& path,bool deletion);
  void fanOut(Source source);
};
}
