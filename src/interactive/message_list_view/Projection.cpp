#include "interactive/message_list_view/Projection.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::message_list_view {
namespace {
void need(const auto& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Message list renderer required: ")+name);
}
}
ftxui::Element projectSurface(const SurfacePlan& plan,const ProjectionImports& imports){
  ftxui::Elements elements;
  if(plan.showLogo){need(imports.renderLogo,"logo");elements.push_back(imports.renderLogo());}
  const auto divider=[&](DividerKind kind,std::ptrdiff_t count){
    need(imports.renderDivider,"divider");
    return imports.renderDivider({kind,count,plan.options.columns});
  };
  if(plan.showTruncated)elements.push_back(divider(DividerKind::ShowPrevious,plan.prepared->hiddenMessageCount));
  if(plan.showAll)elements.push_back(divider(DividerKind::HidePrevious,plan.prepared->hiddenMessageCount));
  const auto piece=[](const markdown::MarkdownPiece& part){
    return part.gap?ftxui::vbox({ftxui::text(""),part.element}):part.element;
  };
  const auto pieces=[](const std::vector<markdown::MarkdownPiece>* parts){
    return parts&&!parts->empty();
  };
  const auto thinking=[&](std::size_t part)->ftxui::Element{
    if(pieces(imports.thinkingPieces)){
      // The rows projectThinking draws for an expanded block, one piece each.
      auto body=ftxui::hbox({ftxui::text("  "),
        piece(imports.thinkingPieces->at(part))|ftxui::xflex});
      if(part)return body;
      return ftxui::vbox({ftxui::text(""),cards_assistant::thinkingHeading(
        imports.messages.assistant.projection),ftxui::text(""),std::move(body)});
    }
    const Json block={{"type","thinking"},{"thinking",plan.options.streamingThinking->thinking}};
    if(auto body=cards_assistant::projectAssistantThinkingBlock(block,false,true,
        plan.options.verbose,false,imports.messages.assistant.projection))
      return ftxui::vbox({ftxui::text(""),*body});
    return ftxui::vbox(ftxui::Elements{});
  };
  const auto response=[&](std::size_t part)->ftxui::Element{
    const auto& assistant=imports.messages.assistant.projection;
    const bool split=pieces(imports.responsePieces);
    if(!split)need(imports.renderStreamingMarkdown,"streaming Markdown");
    auto content=split?piece(imports.responsePieces->at(part)):
      imports.renderStreamingMarkdown(*plan.options.streamingText,false);
    auto lead=part?ftxui::text(""):ftxui::text(assistant.dotGlyph)|
      ftxui::color(assistant.palette.text);
    auto body=ftxui::hbox({std::move(lead)|ftxui::size(ftxui::WIDTH,ftxui::EQUAL,2),
      std::move(content)|ftxui::xflex});
    if(part)return body;
    return ftxui::vbox({ftxui::text(""),std::move(body)});
  };
  const auto queued=[&](std::size_t index){
    need(imports.renderQueuedInput,"queued poetry");
    return imports.renderQueuedInput(plan.options.queuedInputs.at(index),index==0,
      plan.options.columns);
  };
  const RowRenderer renderRow=[&](std::size_t index){
    const auto& row=plan.rows.at(index);
    if(row.kind==SurfaceRow::Kind::StreamingThinking)return thinking(row.part);
    if(row.kind==SurfaceRow::Kind::StreamingText)return response(row.part);
    if(row.kind==SurfaceRow::Kind::QueuedInput)return queued(row.part);
    auto services=imports.messages;
    ftxui::Elements content;
    if(row.unseenCount)content.push_back(ftxui::vbox({ftxui::text(""),
      divider(DividerKind::Unseen,*row.unseenCount)}));
    auto view=message_view::prepareRow(row.request,plan.messageImports);
    if(auto rendered=message_view::projectRow(view,services))content.push_back(*rendered);
    return ftxui::vbox(std::move(content));
  };
  if(plan.options.virtualScroll){
    need(imports.renderVirtualList,"virtual list");
    elements.push_back(imports.renderVirtualList(plan,renderRow));
  }else for(std::size_t index=0;index<plan.rows.size();++index)elements.push_back(renderRow(index));
  if(!plan.options.thinkingInPlace){
    if(plan.showStreamingText)
      for(std::size_t part=0;part<std::max<std::size_t>(1,plan.options.streamingTextParts);++part)
        elements.push_back(response(part));
    if(plan.showStreamingThinking)
      for(std::size_t part=0;part<std::max<std::size_t>(1,plan.options.streamingThinkingParts);++part)
        elements.push_back(thinking(part));
    for(std::size_t index=0;index<plan.options.queuedInputs.size();++index)
      elements.push_back(queued(index));
  }
  return ftxui::vbox(std::move(elements));
}
} // namespace batbox::interactive::message_list_view
