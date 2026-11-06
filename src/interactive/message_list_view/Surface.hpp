#pragma once
#include "interactive/MessageListPipeline.hpp"
#include "interactive/message_view/RowPlan.hpp"
#include <algorithm>
#include <string_view>
#include <unordered_set>

namespace batbox::interactive::message_list_view {
struct UnseenDivider {std::string firstUnseenUuid;int count=0;};
// Whether value::slice(text,0,24)==prefix. Row keys and uuids are ASCII, so
// they are compared in place instead of converted for every row.
inline bool hasUuidPrefix(std::string_view text,const std::string& prefix){
  const auto length=std::min<std::size_t>(24,text.size());
  for(std::size_t i=0;i<length;++i)
    if(static_cast<unsigned char>(text[i])>=0x80)
      return value::slice(std::string(text),0,24)==prefix;
  return text.substr(0,length)==prefix;
}
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
  // Leader poetry sent while the turn runs, drawn after the live rows.
  std::vector<QueuedInput> queuedInputs;
  // Rows the live reasoning and answer are split into (see MarkdownPiece).
  std::size_t streamingThinkingParts=1,streamingTextParts=1;
};
enum class ProgressState {Indeterminate,Completed};
class SurfaceState {
public:
  bool thinkingVisible(const std::shared_ptr<const StreamingThinking>& thinking,double nowMs);
  void committed(bool progressEnabled,bool hasGadgetsInProgress,
    const std::function<void(std::optional<ProgressState>)>& progress);
  void unmounted(const std::function<void(std::optional<ProgressState>)>& progress);
  // latestBashOutputUUID for this prepared list, scanned once per list.
  const Json& latestBashOutput(const std::shared_ptr<const PreparedMessageList>& prepared);
private:
  std::weak_ptr<const PreparedMessageList> bashSource_;
  Json bashOutput_=nullptr;
  std::shared_ptr<const StreamingThinking> previousThinking_;
  bool thinkingVisible_=false;
  std::optional<ProgressState> previousProgress_;
};
struct SurfaceRow {
  std::string key;
  message_view::RowRequest request;
  std::optional<int> unseenCount;
  enum class Kind {Message,StreamingThinking,StreamingText,QueuedInput};
  Kind kind=Kind::Message;
  // The piece of a live row (StreamingThinking, StreamingText) or the index
  // into SurfaceOptions::queuedInputs (QueuedInput).
  std::size_t part=0;
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
SurfacePlan prepareSurface(std::shared_ptr<const PreparedMessageList> prepared,
  SurfaceOptions options,SurfaceState& state,double nowMs,
  const message_view::MessageImports& imports,const MessageDecisionImports& decisions);
} // namespace batbox::interactive::message_list_view
