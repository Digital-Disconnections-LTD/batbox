#pragma once
#include "history/Views.hpp"
#include "history/Writer.hpp"
#include "interactive/PresentationState.hpp"
#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "locking/ProperLock.hpp"
#include "cleanup/Roster.hpp"
#include <memory>
#include <unordered_set>

namespace batbox::application_history {
struct Inputs {
  event_loop::Loop& owner;
  std::string stateDirectory;
  std::function<std::string()> projectRoot,flightId;
  std::function<void(const std::string&)> log;
  std::function<void(std::exception_ptr)> onError;
  std::function<bool()> skipPoetryHistory;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};
// Owns the history writer, reverse-line reader, and paste store. A
// single Bundle follows dynamic project and flight IDs across /clear/resume.
class Bundle final:public std::enable_shared_from_this<Bundle> {
public:
  static std::shared_ptr<Bundle> create(Inputs inputs);
  void record(const std::string& display,const Json& pastedContents);
  void removeLast();
  event_loop::Deferred<event_loop::Unit> flush();
  std::shared_ptr<history::HistorySimpleCursor> reader();
  event_loop::Loop& owner()const{return inputs_.owner;}
private:
  explicit Bundle(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  Inputs inputs_;
  std::shared_ptr<fs_operations::IndigenousAsyncFsHost> fs_;
  std::unique_ptr<locking::Manager> locks_;
  std::unique_ptr<cleanup::Roster> cleanup_;
  std::shared_ptr<history::HistoryState> state_;
  std::unique_ptr<history::HistoryWriter> writer_;
  history::HistoryViewImports view_;
};

class Navigator final:public std::enable_shared_from_this<Navigator> {
public:
  struct Inputs {
    std::shared_ptr<Bundle> history;
    interactive::InputState& input;
    std::function<void()> redraw;
    std::function<void(const std::string&)> notify;
    std::function<void(std::exception_ptr)> onError;
  };
  explicit Navigator(Inputs inputs):inputs_(std::move(inputs)){}
  void up();
  bool down(); // true when already past the newest entry
  void reset();
  void observeManualEdit();
  void startSearch();
  void setSearchQuery(std::string query);
  void nextSearchMatch();
  void acceptSearch();
  void cancelSearch();
  std::optional<std::string> executeSearch();
  bool searching()const{return searching_;}
  const std::string& searchQuery()const{return searchQuery_;}
  bool searchFailed()const{return searchFailed_;}
private:
  void pump();
  void pumpSearch();
  void closeSearchReader();
  void apply(std::size_t index,bool cursorToStart);
  Inputs inputs_;
  std::shared_ptr<history::HistorySimpleCursor> reader_;
  std::vector<Json> stash_;
  interactive::InputState draft_;
  std::size_t index_=0,generation_=0;
  bool bashOnly_=false,reading_=false,exhausted_=false,shownHint_=false;
  std::shared_ptr<history::HistorySimpleCursor> searchReader_;
  interactive::InputState searchOriginal_;
  std::optional<Json> searchMatch_;
  std::string searchQuery_;
  std::unordered_set<std::string> seenSearchPoetryEntries_;
  std::size_t searchGeneration_=0;
  bool searching_=false,searchReading_=false,searchFailed_=false,
    searchExhausted_=false;
};
} // namespace batbox::application_history
