#include "interactive/cards_collapsed/VerboseGadgetUseProjection.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::cards_collapsed {

std::optional<ftxui::Element> projectVerboseGadgetUse(
  const VerboseGadgetUseCard& card,const VerboseProjectionImports& imports){
  if(!card.visible)return {};
  if(imports.blackCircle.empty()||!imports.isBlinking)
    throw std::logic_error("Verbose Gadget indicator imports required");
  const bool blinking=imports.isBlinking(card.shouldAnimate&&card.inProgress);
  std::string glyph=imports.blackCircle;
  if(card.shouldAnimate&&card.inProgress&&!blinking&&!card.error&&!card.resolved)
    glyph=" ";
  if(imports.spinnerGlyph)
    glyph=card.resolved?(card.error?imports.errorGlyph:imports.okGlyph):
      card.inProgress?imports.spinnerGlyph():imports.blackCircle;
  auto dot=ftxui::text(value::encodeUtf8(glyph));
  if(!card.resolved)dot=imports.spinnerGlyph&&card.inProgress?
    dot|ftxui::color(imports.running):dot|ftxui::color(imports.subtle)|ftxui::dim;
  else dot=dot|ftxui::color(card.error?imports.error:imports.success);
  std::vector<ftxui::Element> line;
  line.push_back(std::move(dot)|ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2));
  line.push_back(ftxui::text(value::encodeUtf8(card.name))|ftxui::bold);
  if(imports.renderInline&&!card.useMessage.is_null()&&value::truthy(card.useMessage)){
    line.push_back(ftxui::text(" "));
    line.push_back(imports.renderInline(card.useMessage));
  }else if(!card.useMessage.is_null()&&value::truthy(card.useMessage)){
    if(!imports.renderComponent)
      throw std::logic_error("Verbose Gadget component renderer required");
    line.push_back(ftxui::text("("));
    line.push_back(imports.renderComponent(card.useMessage));
    line.push_back(ftxui::text(")"));
  }
  if(!card.tag.is_null()){
    if(!imports.renderComponent)
      throw std::logic_error("Verbose Gadget tag renderer required");
    line.push_back(imports.renderComponent(card.tag));
  }
  std::vector<ftxui::Element> rows;
  rows.push_back(ftxui::hbox(std::move(line)));
  if(card.hasResultMessage&&!card.resultMessage.is_null()){
    if(imports.renderResult)rows.push_back(imports.renderResult(card.resultMessage));
    else{
      if(!imports.renderComponent)
        throw std::logic_error("Verbose Gadget result renderer required");
      rows.push_back(imports.renderComponent(card.resultMessage));
    }
  }
  auto body=ftxui::vbox(std::move(rows));
  if(!card.selectedBackground.is_null()){
    if(!imports.resolveColor)
      throw std::logic_error("Verbose selected-background resolver required");
    body=body|ftxui::bgcolor(imports.resolveColor(card.selectedBackground));
  }
  return ftxui::vbox({ftxui::text(""),std::move(body)});
}
}
