#pragma once
#include "interactive/MessageListPipeline.hpp"
#include "interactive/message_view/RowPlan.hpp"
#include <unordered_set>

namespace batbox::interactive::message_list_view {
struct UnseenDivider {std::string firstUnseenUuid;int count=0;};
struct Cursor {std::string uuid;bool expanded=false;};
struct SurfaceOptions {
  std::string screen="prompt",conversationId;
  int columns=80;
  bool verbose=false,isLoading=false,hidePastThinking=false,hideLogo=false;
  bool briefOnly=false,showAllInTranscript=false,disableRenderCap=false,virtualScroll=false;
  // Indigenous transcript presentation: completed reasoning stays with its turn,
  // and transient blocks participate in the same measured scrolling list.
  bool thinkingInPlace=false;
  std::optional<std::pair<std::size_t,std::size_t>> renderRange;
  std::optional<bool> gadgetUiContinueAnimation;
  std::size_t permissionQueueSize=0;
  bool messageSelectorVisible=false;
  std::shared_ptr<const StreamingThinking> streamingThinking;
  std::optional<std::string> streamingText;
  GadgetIDs inProgressGadgetUseIDs,streamingGadgetUseIDs;
  std::optional<UnseenDivider> unseenDivider;
  std::optional<Cursor> cursor;
};
enum class ProgressState {Indeterminate,Completed};
class SurfaceState {
public:
  bool thinkingVisible(const std::shared_ptr<const StreamingThinking>& thinking,double nowMs);
  void toggleExpanded(const Json& message);
  bool expanded(const Json& message)const;
  void committed(bool progressEnabled,bool hasGadgetsInProgress,
    const std::function<void(std::optional<ProgressState>)>& progress);
  void unmounted(const std::function<void(std::optional<ProgressState>)>& progress);
private:
  std::shared_ptr<const StreamingThinking> previousThinking_;
  bool thinkingVisible_=false;
  std::unordered_set<std::string> expandedKeys_;
  std::optional<ProgressState> previousProgress_;
};
struct SurfaceRow {
  std::string key;
  message_view::RowRequest request;
  bool selected=false;
  std::optional<int> unseenCount;
  enum class Kind {Message,StreamingThinking,StreamingText};
  Kind kind=Kind::Message;
};
struct SurfacePlan {
  std::shared_ptr<const PreparedMessageList> prepared;
  SurfaceOptions options;
  message_view::MessageImports messageImports;
  std::vector<SurfaceRow> rows;
  Json lastThinkingBlockId=nullptr,latestBashOutputUUID=nullptr;
  bool showLogo=false,showTruncated=false,showAll=false;
  bool showStreamingText=false,showStreamingThinking=false;
};
Json lastThinkingBlockId(const MessageRefs& messages,bool hidePast,bool streamingVisible);
Json latestBashOutputUUID(const MessageRefs& messages);
bool isItemClickable(const Json& message,const MessageLookups& lookups,
  const gadgets::Gadgets& gadgets,const std::function<bool(const Json&)>& isAdvisorBlock);
SurfacePlan prepareSurface(std::shared_ptr<const PreparedMessageList> prepared,
  SurfaceOptions options,SurfaceState& state,double nowMs,
  const message_view::MessageImports& imports,const MessageDecisionImports& decisions);
} // namespace batbox::interactive::message_list_view
