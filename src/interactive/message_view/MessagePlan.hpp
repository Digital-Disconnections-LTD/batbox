#pragma once
#include "interactive/MessageGrouping.hpp"
#include "interactive/MessageLookups.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::message_view {
enum class MessageKind {Hidden,Attachment,Assistant,User,CompactSummary,
  CompactBoundary,SnipBoundary,LocalCommand,System,GroupedGadgets,CollapsedReadSearch};
enum class BlockKind {Hidden,UserText,UserImage,GadgetResult,AssistantText,
  AssistantGadgetUse,RedactedThinking,Thinking,Advisor};
struct MessageOptions {
  bool addMargin=true,verbose=false,transcriptMode=false,shouldAnimate=false;
  bool shouldShowDot=true,isActiveCollapsedGroup=false,isUserContinuation=false;
  bool isStatic=false;
  bool showThinkingInPlace=false;
  int columns=80;
  std::optional<int> containerWidth;
  Json width=Json(Json::value_t::discarded);
  Json style=Json(Json::value_t::discarded);
  Json lastThinkingBlockId=Json(Json::value_t::discarded);
  Json latestBashOutputUUID=Json(Json::value_t::discarded);
  GadgetIDs inProgressGadgetUseIDs;
  MessageRefs progressMessages;
};
struct MessageImports {
  std::function<bool()> fullscreen;
  std::function<bool(const std::string&)> feature;
  std::function<bool(const Json&)> isSnipBoundary,isSnipMarker;
  std::function<bool(const Json&)> isConnectorTextBlock,isAdvisorBlock;
  std::function<void(const std::string&)> logError;
};
struct BlockPlan {
  BlockKind kind=BlockKind::Hidden;
  GadgetBlockRef original;
  Json replacement=Json(Json::value_t::discarded);
  bool addMargin=false,verbose=false,transcriptMode=false,hideInTranscript=false;
  bool shouldShowDot=false,shouldAnimate=false;
  Json width=Json(Json::value_t::discarded),imageIndex=nullptr;
  std::string thinkingBlockId;
  std::size_t inProgressGadgetCallCount=0;
  const Json& value()const{return replacement.is_discarded()?original.value():replacement;}
};
struct MessagePlan {
  MessageKind kind=MessageKind::Hidden;
  RenderItem item;
  MessageOptions options;
  std::vector<BlockPlan> blocks;
  bool expandShellOutput=false;
  explicit MessagePlan(RenderItem value,MessageOptions settings):
    item(std::move(value)),options(std::move(settings)){}
};
MessagePlan prepareMessage(RenderItem item,MessageOptions options,
  const MessageImports& imports);
BlockPlan prepareUserBlock(const GadgetBlockRef& block,const Json& imageIndex,
  const MessageOptions& options);
BlockPlan prepareAssistantBlock(const GadgetBlockRef& block,const std::string& thinkingId,
  const MessageOptions& options,const MessageImports& imports);
bool hasThinkingContent(const Json& message);
bool sameThinkingMarker(const Json& previous,const Json& next);
bool sameMessageView(const MessagePlan& previous,const MessagePlan& next);
} // namespace batbox::interactive::message_view
