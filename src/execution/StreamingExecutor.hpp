#pragma once
#include "execution/GadgetExecution.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace batbox::execution {

class StreamingGadgetExecutor {
 public:
  StreamingGadgetExecutor(Context context,Services& services);
  ~StreamingGadgetExecutor();
  StreamingGadgetExecutor(const StreamingGadgetExecutor&)=delete;
  StreamingGadgetExecutor& operator=(const StreamingGadgetExecutor&)=delete;

  void discard();
  void addGadget(const Json& block,const Json& assistantMessage);
  std::vector<Update> getCompletedResults();
  void getRemainingResults(const std::function<void(const Update&)>& yield);
  Context getUpdatedContext() const;

 private:
  enum class Status { Queued,Executing,Completed,Yielded };
  struct Tracked {
    std::string id;
    Json block;
    Json assistantMessage;
    Status status=Status::Queued;
    bool concurrencySafe=false;
    std::thread worker;
    std::stop_source childStop;
    std::unique_ptr<std::stop_callback<std::function<void()>>> parentStop;
    std::unique_ptr<std::stop_callback<std::function<void()>>> siblingStop;
    std::deque<Json> pendingProgress;
    std::vector<Json> results;
    std::vector<std::function<Json(const Json&)>> contextModifiers;
    std::vector<std::function<Json(const Json&)>> appStateModifiers;
  };
  void processQueue();
  void executeGadget(Tracked& gadget);
  std::vector<Update> getCompletedResultsLocked(std::vector<std::string>& completedIDs);
  bool unfinishedLocked() const;
  bool executingLocked() const;
  bool completedLocked() const;
  bool progressLocked() const;
  std::string abortReasonLocked(const Tracked& gadget) const;
  Json syntheticError(const Tracked& gadget,const std::string& reason) const;
  std::string gadgetDescription(const Tracked& gadget) const;
  std::string interruptBehavior(const Tracked& gadget) const;
  void updateInterruptibleStateLocked();
  Context context_;
  Services& services_;
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  std::vector<std::unique_ptr<Tracked>> gadgets_;
  std::stop_source siblingStop_;
  bool hasErrored_=false;
  std::string erroredDescription_;
  bool discarded_=false;
  std::exception_ptr failure_;
};

} // namespace batbox::execution
