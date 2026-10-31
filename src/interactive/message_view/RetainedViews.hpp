#pragma once
#include "interactive/message_view/Projection.hpp"
#include "interactive/cards_collapsed/CollapsedBinding.hpp"
#include "interactive/cards_collapsed/MinHintDisplay.hpp"
#include "interactive/cards_gadget_binding/GadgetChildBinding.hpp"
#include <unordered_map>
#include <unordered_set>

namespace batbox::interactive::message_view {
struct RetainedViewServices {
  cards_gadget_binding::GadgetChildBindingServices result;
  cards_collapsed::CollapsedServices collapsed;
  cards_collapsed::CollapsedProjectionImports collapsedProjection;
  cards_collapsed::VerboseGadgetUseServices verboseGadgets;
  cards_collapsed::VerboseProjectionImports verboseProjection;
};
// One owner per indigenous message list. Driver's successful-frame callback runs
// committed(); beginFrame() starts the next render's mounted-card collection.
class RetainedViews final:public std::enable_shared_from_this<RetainedViews> {
public:
  static std::shared_ptr<RetainedViews> create(event_loop::Loop& loop,
    std::function<double()> nowMs,std::function<void()> requestRedraw);
  ~RetainedViews();
  void beginFrame(std::string conversationId);
  ProjectionServices bind(ProjectionServices base,RetainedViewServices services);
  void committed();
  void unmountAll();
private:
  struct CollapsedEntry {
    cards_collapsed::CollapsedState state;
    std::shared_ptr<cards_collapsed::MinHintDisplay> hint;
  };
  RetainedViews(event_loop::Loop& loop,std::function<double()> nowMs,
    std::function<void()> requestRedraw);
  std::string key(const MessagePlan& message,const BlockPlan* block=nullptr)const;
  event_loop::Loop& loop_;
  std::function<double()> nowMs_;
  std::function<void()> requestRedraw_;
  std::string conversationId_;
  cards_gadget_binding::RetainedSuccessStates successes_;
  std::unordered_set<std::string> previousSuccesses_,activeSuccesses_,activeCollapsed_;
  std::unordered_map<std::string,std::shared_ptr<CollapsedEntry>> collapsed_;
  std::vector<std::function<void()>> effects_;
};
} // namespace batbox::interactive::message_view
