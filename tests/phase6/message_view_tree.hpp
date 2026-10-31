#pragma once
#include "message_view_fixture.hpp"
namespace message_view_fixture {
inline Json blockTree(const MessagePlan& plan,const BlockPlan& block){
  if(block.kind==BlockKind::Hidden)return nullptr;
  Json props={{"addMargin",block.addMargin}};
  if(block.kind==BlockKind::UserImage){props["imageId"]=block.imageIndex;return node("UserImageMessage",props);}
  if(block.kind==BlockKind::RedactedThinking)return node("AssistantRedactedThinkingMessage",props);
  props["verbose"]=block.verbose;
  if(block.kind==BlockKind::Advisor){
    props["block"]=block.value();props["shouldAnimate"]=block.shouldAnimate;
    put(props,"advisorModel",field(*plan.item.message,"advisorModel"));return node("AdvisorMessage",props);
  }
  props["param"]=block.value();
  if(block.kind==BlockKind::UserText){
    props["isTranscriptMode"]=block.transcriptMode;
    if(plan.kind!=MessageKind::LocalCommand){
      put(props,"timestamp",field(*plan.item.message,"timestamp"));
      put(props,"planContent",field(*plan.item.message,"planContent"));
    }
    return node("UserTextMessage",props);
  }
  if(block.kind==BlockKind::AssistantText){
    props["shouldShowDot"]=block.shouldShowDot;put(props,"width",block.width);
    return node("AssistantTextMessage",props);
  }
  props["isTranscriptMode"]=block.transcriptMode;
  if(block.kind==BlockKind::Thinking){props["hideInTranscript"]=block.hideInTranscript;return node("AssistantThinkingMessage",props);}
  props["progressMessagesForMessage"]=progressJson(plan.options.progressMessages);
  if(block.kind==BlockKind::GadgetResult){
    props.erase("addMargin");props["message"]=*plan.item.message;
    put(props,"style",plan.options.style);put(props,"width",block.width);
    return node("UserToolResultMessage",props);
  }
  props["inProgressToolUseIDs"]=idsJson(plan.options.inProgressGadgetUseIDs);
  props["shouldAnimate"]=block.shouldAnimate;props["shouldShowDot"]=block.shouldShowDot;
  props["inProgressToolCallCount"]=block.inProgressGadgetCallCount;
  return node("AssistantToolUseMessage",props);
}
inline Json messageTree(const MessagePlan& plan){
  const auto& o=plan.options;const auto& msg=*plan.item.message;
  Json common={{"addMargin",o.addMargin},{"verbose",o.verbose},{"isTranscriptMode",o.transcriptMode}};
  switch(plan.kind){
    case MessageKind::Hidden:return nullptr;
    case MessageKind::Attachment:
      common["attachment"]=msg.at("attachment");return node("AttachmentMessage",common);
    case MessageKind::CompactSummary:return node("CompactSummary",{{"message",msg},{"screen",o.transcriptMode?"transcript":"prompt"}});
    case MessageKind::CompactBoundary:return node("CompactBoundaryMessage");
    case MessageKind::SnipBoundary:return node("SnipBoundaryMessage",{{"message",msg}});
    case MessageKind::System:common["message"]=msg;return node("SystemTextMessage",common);
    case MessageKind::LocalCommand:return blockTree(plan,plan.blocks.front());
    case MessageKind::GroupedGadgets:
      return node("GroupedToolUseContent",{{"message",materializeRenderItem(plan.item)},
        {"shouldAnimate",o.shouldAnimate},{"inProgressToolUseIDs",idsJson(o.inProgressGadgetUseIDs)}});
    case MessageKind::CollapsedReadSearch:
      return node("OffscreenFreeze",Json::object(),Json::array({node("CollapsedReadSearchContent",
        {{"message",materializeRenderItem(plan.item)},{"shouldAnimate",o.shouldAnimate},
        {"inProgressToolUseIDs",idsJson(o.inProgressGadgetUseIDs)},{"verbose",o.verbose||o.transcriptMode},{"isActiveGroup",o.isActiveCollapsedGroup}})}));
    case MessageKind::Assistant:case MessageKind::User:break;
  }
  Json children=Json::array();for(const auto& block:plan.blocks)children.push_back(blockTree(plan,block));
  Json box=node("Box",{{"flexDirection","column"},{"width",o.containerWidth?Json(*o.containerWidth):Json("100%")}},children);
  return plan.expandShellOutput?node("ExpandShellOutputProvider",Json::object(),Json::array({box})):box;
}
inline Json rowTree(const RowPlan& plan){
  const auto& o=plan.message.options;
  Json props={{"message",materializeRenderItem(plan.request.item)},
    {"addMargin",o.addMargin},{"verbose",o.verbose},
    {"inProgressToolUseIDs",idsJson(o.inProgressGadgetUseIDs)},
    {"progressMessagesForMessage",progressJson(o.progressMessages)},
    {"shouldAnimate",o.shouldAnimate},{"shouldShowDot",o.shouldShowDot},
    {"isTranscriptMode",o.transcriptMode},{"isStatic",o.isStatic},
    {"isActiveCollapsedGroup",o.isActiveCollapsedGroup},{"isUserContinuation",o.isUserContinuation}};
  if(o.containerWidth)props["containerWidth"]=*o.containerWidth;
  put(props,"lastThinkingBlockId",o.lastThinkingBlockId);
  put(props,"latestBashOutputUUID",o.latestBashOutputUUID);
  auto content=node("Message",props);
  if(plan.hasMetadata){
    const Json metadataProps={{"message",*plan.displayMessage},{"isTranscriptMode",true}};
    auto metadata=node("Box",{{"flexDirection","row"},{"justifyContent","flex-end"},{"gap",1},{"marginTop",1}},
      Json::array({node("MessageTimestamp",metadataProps),node("MessageModel",metadataProps)}));
    content=node("Box",{{"width",o.columns},{"flexDirection","column"}},Json::array({metadata,content}));
  }
  return node("OffscreenFreeze",Json::object(),Json::array({content}));
}
} // namespace message_view_fixture
