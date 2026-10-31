#include "interactive/message_list_view/Projection.hpp"
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
  const auto thinking=[&]()->ftxui::Element{
    const Json block={{"type","thinking"},{"thinking",plan.options.streamingThinking->thinking}};
    if(auto body=cards_assistant::projectAssistantThinkingBlock(block,false,true,
        plan.options.verbose,false,imports.messages.assistant.projection))
      return ftxui::vbox({ftxui::text(""),*body});
    return ftxui::vbox(ftxui::Elements{});
  };
  const auto response=[&]()->ftxui::Element{
    const auto& assistant=imports.messages.assistant.projection;
    need(imports.renderStreamingMarkdown,"streaming Markdown");
    auto body=ftxui::hbox({
      ftxui::text(assistant.dotGlyph)|ftxui::color(assistant.palette.text)|
        ftxui::size(ftxui::WIDTH,ftxui::EQUAL,2),
      imports.renderStreamingMarkdown(*plan.options.streamingText,false)|ftxui::xflex});
    return ftxui::vbox({ftxui::text(""),std::move(body)});
  };
  const RowRenderer renderRow=[&](std::size_t index){
    const auto& row=plan.rows.at(index);
    if(row.kind==SurfaceRow::Kind::StreamingThinking)return thinking();
    if(row.kind==SurfaceRow::Kind::StreamingText)return response();
    auto services=imports.messages;services.selected=row.selected;
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
    if(plan.showStreamingText)elements.push_back(response());
    if(plan.showStreamingThinking)elements.push_back(thinking());
  }
  return ftxui::vbox(std::move(elements));
}
} // namespace batbox::interactive::message_list_view
