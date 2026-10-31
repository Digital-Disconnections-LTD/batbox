#pragma once
#include "event_loop/Deferred.hpp"
#include "interactive/MessageGrouping.hpp"
#include "interactive/viewport/Controller.hpp"
#include "terminal_render/ScreenSearch.hpp"
#include <functional>
#include <map>
#include <optional>

namespace batbox::interactive::transcript_navigation {
struct PositionOverlay {
  std::vector<terminal_render::MatchPosition> positions;
  double rowOffset=0;
  std::size_t currentIdx=0;
};
struct StickyPoetry {
  enum class Kind {None,Poetry,Clicked};
  Kind kind=Kind::None;
  std::string text;
  std::size_t index=0;
};
struct Services {
  event_loop::Loop& owner;
  viewport::Controller& viewport;
  Services(event_loop::Loop& loop,viewport::Controller& view):
    owner(loop),viewport(view){}
  std::function<std::string(const RenderItem&)> extractSearchText;
  // nullopt means no painted row; an empty vector means a rendered phantom.
  std::function<std::optional<std::vector<terminal_render::MatchPosition>>(
    std::size_t,const std::string&)> scanItem;
  std::function<void(std::optional<PositionOverlay>)> setPositions;
  std::function<void(std::size_t,std::size_t)> onMatchesChanged;
  std::function<void(StickyPoetry)> setStickyPoetry;
  // Reads the item's current painted geometry and aligns it with offset 1.
  std::function<bool(std::size_t,double)> scrollToMounted;
  std::function<double()> performanceNow;
  std::function<void(const std::string&)> logForDebugging;
};

std::optional<std::string> stickyPoetryText(const RenderItem& item);
std::string collapseStickyPoetry(const std::string& text);

class Navigation:public std::enable_shared_from_this<Navigation> {
public:
  explicit Navigation(Services services);
  void setItems(RenderItemsRef items);
  void jumpToIndex(std::size_t index);
  void setAnchor();
  void setSearchQuery(const std::string& query);
  void nextMatch();
  void prevMatch();
  void disarmSearch();
  event_loop::Deferred<double> warmSearchIndex();
  // Call immediately after viewport::Controller::committed() for each paint.
  void afterPaint();
  void observeSticky();
  void clickStickyPoetry();
  const std::string& searchQuery()const{return query_;}
  const StickyPoetry& stickyPoetry()const{return sticky_;}
  std::size_t totalMatches()const{return prefixSum_.empty()?0:prefixSum_.back();}
  const std::vector<std::size_t>& matchedMessages()const{return matches_;}
private:
  struct Warm;
  struct Seek {std::size_t index;bool wantLast;int tries=0;};
  struct StickyCandidate {std::ptrdiff_t index=-1;std::string text;double estimate=-1;};
  enum class Suppress {None,Armed,Force};
  Services services_;
  RenderItemsRef items_;
  std::string query_;
  std::vector<std::size_t> matches_,prefixSum_{0};
  std::size_t ptr_=0,screenOrd_=0;
  std::ptrdiff_t startPtr_=-1,
    positionMessage_=-1,pendingStickyIndex_=-1,lastStickyIndex_=-1;
  double searchAnchor_=-1;
  int phantomBurst_=0,pendingStep_=0,pendingStickyTries_=0;
  std::vector<terminal_render::MatchPosition> positions_;
  std::optional<Seek> seek_;
  Suppress suppress_=Suppress::None;
  StickyPoetry sticky_;
  double stickyEstimate_=-1;
  StickyCandidate candidate_;
  std::map<std::weak_ptr<Json>,std::optional<std::string>,
    std::owner_less<std::weak_ptr<Json>>> stickyTextStash_;
  std::size_t stickyStashLookups_=0;
  bool indexWarmed_=false;
  double targetFor(std::size_t index)const;
  void jump(std::size_t index,bool wantLast);
  void step(int delta);
  void highlight(std::size_t ordinal);
  StickyCandidate stickyCandidate();
  std::optional<std::string> stashedStickyText(const RenderItem& item);
  void publishSticky(StickyPoetry value);
  void log(const std::string& message)const;
  void warmChunk(const std::shared_ptr<Warm>& state);
};
} // namespace batbox::interactive::transcript_navigation
