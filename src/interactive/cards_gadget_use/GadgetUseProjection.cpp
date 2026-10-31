#include "interactive/cards_gadget_use/GadgetUseProjection.hpp"
#include "interactive/cards_gadget_result/HookProgressProjection.hpp"
#include "runtime/Encoding.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::interactive::cards_gadget_use {
namespace {
ftxui::Element component(const Json& value,const UseProjectionImports& imports){
  if(!imports.renderGadgetMessage)
    throw std::logic_error("Gadget component renderer required");
  return imports.renderGadgetMessage(value);
}
ftxui::Element response(ftxui::Element child,ftxui::Color subtle){
  return ftxui::hbox({ftxui::text("  ⎿  ")|
      ftxui::color(subtle)|ftxui::dim,std::move(child)})|
      ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,1);
}
std::optional<ftxui::Element> progress(const GadgetUseCard& card,
                                       const UseProjectionImports& imports){
  if(card.progressKind==GadgetUseCard::ProgressKind::None)return {};
  if(card.progressKind==GadgetUseCard::ProgressKind::WaitingPermission)
    return response(ftxui::text("Waiting for permission…")|
      ftxui::color(imports.subtle)|ftxui::dim,imports.subtle);
  std::vector<ftxui::Element> rows;
  if(auto hook=cards_gadget_result::projectHookProgress(
      card.preGadgetHook,imports.subtle))rows.push_back(std::move(*hook));
  if(!card.progress.is_null())rows.push_back(component(card.progress,imports));
  if(rows.empty())return {};
  return ftxui::vbox(std::move(rows));
}
}

std::optional<ftxui::Element> projectGadgetUse(const GadgetUseCard& card,
                                             const UseProjectionImports& imports){
  if(!card.visible)return {};
  std::vector<ftxui::Element> rows;
  if(!card.transparent){
    std::vector<ftxui::Element> header;
    if(card.showDot){
      if(imports.blackCircle.empty())
        throw std::logic_error("Gadget-use indicator glyph required");
      std::string glyph=imports.blackCircle;
      if(!card.queued){
        if(!imports.isBlinking)
          throw std::logic_error("Gadget-use blink provider required");
        const bool blinking=imports.isBlinking(card.animate);
        if(card.animate&&!blinking&&!card.error&&!card.resolved)glyph=" ";
      }
      auto dot=ftxui::text(value::encodeUtf8(glyph));
      if(card.queued||!card.resolved)dot=dot|ftxui::color(imports.subtle)|ftxui::dim;
      else dot=dot|ftxui::color(card.error?imports.error:imports.success);
      header.push_back(std::move(dot)|ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2));
    }
    auto name=ftxui::text(value::encodeUtf8(card.name))|ftxui::bold;
    if(!card.nameBackground.is_null()){
      if(!imports.resolveColor)
        throw std::logic_error("Gadget-use color resolver required");
      name=name|ftxui::bgcolor(imports.resolveColor(card.nameBackground));
    }
    header.push_back(std::move(name));
    if(!card.useText.is_null()&&card.useText!=""){
      header.push_back(ftxui::text("("));
      header.push_back(component(card.useText,imports));
      header.push_back(ftxui::text(")"));
    }
    if(!card.tag.is_null())header.push_back(component(card.tag,imports));
    rows.push_back(ftxui::hbox(std::move(header))|
      ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,card.nameWidth));
  }
  if(auto value=progress(card,imports))rows.push_back(std::move(*value));
  if(!card.queuedMessage.is_null())rows.push_back(component(card.queuedMessage,imports));
  if(rows.empty())return {};
  auto body=ftxui::vbox(std::move(rows));
  if(!card.selectedBackground.is_null()){
    if(!imports.resolveColor)
      throw std::logic_error("Selected-message color resolver required");
    body=body|ftxui::bgcolor(imports.resolveColor(card.selectedBackground));
  }
  if(card.marginTop)body=ftxui::vbox({ftxui::text(""),std::move(body)});
  return body;
}
}
