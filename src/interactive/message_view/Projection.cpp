#include "interactive/message_view/Projection.hpp"
#include <stdexcept>

namespace batbox::interactive::message_view {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Message projection import required: ")+name);
}
Json progressMessages(const MessagePlan& message){
  Json progress=Json::array();
  for(const auto& event:message.options.progressMessages)progress.push_back(*event);
  return progress;
}
std::optional<ftxui::Element> projectBlock(const MessagePlan& message,
    const BlockPlan& block,const ProjectionServices& services){
  if(block.kind==BlockKind::Hidden)return {};
  if(block.kind==BlockKind::UserText){
    auto user=services.user;
    user.poetry.isTranscriptMode=block.transcriptMode;
    user.bashResult.expandShellOutput=user.bashResult.expandShellOutput||message.expandShellOutput;
    user.bashResult.columns=message.options.columns;
    const Json empty=Json::object();
    const auto& owner=message.kind==MessageKind::LocalCommand?empty:*message.item.message;
    return cards::projectUserTextBlock(owner,block.value(),block.addMargin,
      block.verbose,message.options.columns,user);
  }
  if(block.kind==BlockKind::UserImage){
    const auto id=block.imageIndex.is_number()?std::optional<int>(block.imageIndex.get<int>()):std::nullopt;
    return cards_remaining::projectUserImage(cards_remaining::classifyUserImage(id,
      block.addMargin,services.remaining.images),services.remaining.palette);
  }
  if(block.kind==BlockKind::AssistantText){
    auto element=cards_assistant::projectAssistantTextBlock(block.value(),block.verbose,
      block.addMargin,services.selected,block.shouldShowDot,services.assistant);
    if(element&&block.width.is_number())*element=*element|
      ftxui::size(ftxui::WIDTH,ftxui::EQUAL,block.width.get<int>());
    return element;
  }
  if(block.kind==BlockKind::Thinking||block.kind==BlockKind::RedactedThinking)
    return cards_assistant::projectAssistantThinkingBlock(block.value(),block.addMargin,
      block.transcriptMode,block.verbose,block.hideInTranscript,services.assistant.projection);
  if(block.kind==BlockKind::GadgetResult){
    need(services.lookups,"lookups");need(services.gadgetResult,"toolResult");
    auto imports=services.gadgetResult(message,block,*services.lookups,services.gadgets);
    imports.columns=message.options.columns;
    return cards_gadget_result::projectGadgetResultBlock(block.value(),progressMessages(message),
      *services.lookups,services.gadgets,block.verbose,imports);
  }
  if(block.kind==BlockKind::AssistantGadgetUse){
    need(services.lookups,"lookups");
    cards_gadget_use::GadgetUseInputs input;
    input.param=block.value();input.addMargin=block.addMargin;input.gadgets=&services.gadgets;
    input.commands=services.commands;input.verbose=block.verbose;
    input.inProgressGadgetUseIDs=&message.options.inProgressGadgetUseIDs;
    input.progressMessages=progressMessages(message);input.shouldAnimate=block.shouldAnimate;
    input.shouldShowDot=block.shouldShowDot;
    input.inProgressGadgetCallCount=static_cast<int>(block.inProgressGadgetCallCount);
    input.lookups=services.lookups.get();input.transcriptMode=block.transcriptMode;
    return cards_gadget_use::projectGadgetUse(
      cards_gadget_use::classifyGadgetUse(input,services.gadgetUse),services.gadgetUseProjection);
  }
  if(block.kind==BlockKind::Advisor){
    need(services.lookups,"lookups");
    const auto& messageModel=value::property(*message.item.message,"advisorModel");
    const auto model=messageModel.is_string()?std::optional<std::string>(messageModel.get<std::string>()):std::nullopt;
    return cards_remaining::projectAdvisor(cards_remaining::classifyAdvisor(block.value(),
      block.addMargin,services.lookups->resolvedGadgetUseIDs,services.lookups->erroredGadgetUseIDs,
      block.shouldAnimate,block.verbose,model,services.remaining.advisor),services.remaining.palette);
  }
  need(services.otherBlock,"specialized block renderer");
  return services.otherBlock(message,block);
}
}
std::optional<ftxui::Element> projectMessage(const MessagePlan& plan,
    const ProjectionServices& services){
  if(plan.kind==MessageKind::Hidden)return {};
  if(plan.kind==MessageKind::Attachment)
    return cards_remaining::projectAttachment(cards_remaining::classifyAttachment(
      plan.item.message->at("attachment"),plan.options.addMargin,plan.options.verbose,
      plan.options.transcriptMode,services.remaining.attachments),services.remaining.attachmentProjection);
  if(plan.kind==MessageKind::CompactSummary)
    return cards_remaining::projectCompactSummary(cards_remaining::classifyCompactSummary(
      *plan.item.message,plan.options.transcriptMode?"transcript":"prompt",services.remaining.compact),
      services.remaining.palette);
  if(plan.kind==MessageKind::CompactBoundary)
    return cards_remaining::projectCompactBoundary(cards_remaining::compactBoundaryText(
      services.remaining.historyShortcut),services.remaining.palette);
  if(plan.kind==MessageKind::System)
    return cards_system::projectSystemTextMessage(*plan.item.message,
      plan.options.addMargin,plan.options.verbose,plan.options.transcriptMode,services.system);
  if(plan.kind==MessageKind::GroupedGadgets){
    need(services.lookups,"lookups");
    const auto card=cards_gadget_use::renderGroupedGadgetUse(materializeRenderItem(plan.item),
      services.gadgets,*services.lookups,plan.options.inProgressGadgetUseIDs,
      plan.options.shouldAnimate,services.groupedGadgetUse);
    if(card.is_null()||card.is_discarded())return {};
    need(services.gadgetUseProjection.renderGadgetMessage,"grouped Gadget renderer");
    return services.gadgetUseProjection.renderGadgetMessage(card);
  }
  if(plan.kind==MessageKind::CollapsedReadSearch){
    need(services.lookups,"lookups");need(services.collapsed,"collapsed read/search");
    return services.collapsed(plan,*services.lookups,services.gadgets);
  }
  if(plan.kind==MessageKind::Assistant||plan.kind==MessageKind::User||
      plan.kind==MessageKind::LocalCommand){
    ftxui::Elements children;
    for(const auto& block:plan.blocks)
      if(auto child=projectBlock(plan,block,services))children.push_back(*child);
    if(plan.kind==MessageKind::LocalCommand)
      return children.empty()?std::optional<ftxui::Element>{}:children.front();
    auto box=ftxui::vbox(std::move(children));
    if(plan.options.containerWidth)box=box|
      ftxui::size(ftxui::WIDTH,ftxui::EQUAL,*plan.options.containerWidth);
    return box;
  }
  need(services.otherMessage,"specialized message renderer");
  return services.otherMessage(plan);
}
std::optional<ftxui::Element> projectRow(const RowPlan& plan,
    const ProjectionServices& services){
  auto rowServices=services;rowServices.lookups=plan.request.lookups;
  if(!plan.hasMetadata)return projectMessage(plan.message,rowServices);
  ftxui::Elements parts;
  const auto add=[&](const std::optional<MetadataPart>& part){
    if(!part)return;
    if(!parts.empty())parts.push_back(ftxui::text(" "));
    parts.push_back(ftxui::text(part->text)|ftxui::color(services.metadataColor)|
      ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,part->minWidth));
  };
  add(timestampMetadata(*plan.displayMessage,true,services.metadata));
  add(modelMetadata(*plan.displayMessage,true,services.metadata));
  auto metadata=ftxui::hbox({ftxui::filler(),ftxui::hbox(std::move(parts))});
  ftxui::Elements rows{ftxui::text(""),std::move(metadata)};
  auto content=projectMessage(plan.message,rowServices);
  if(content)rows.push_back(*content);
  return ftxui::vbox(std::move(rows))|
    ftxui::size(ftxui::WIDTH,ftxui::EQUAL,plan.request.options.columns);
}
} // namespace batbox::interactive::message_view
