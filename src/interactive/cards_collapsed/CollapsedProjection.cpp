#include "interactive/cards_collapsed/CollapsedProjection.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::cards_collapsed {
namespace {
ftxui::Element indicator(const CollapsedCard& card,
                         const CollapsedProjectionImports& imports){
  if(imports.spinnerGlyph){
    if(card.active&&!card.error)
      return ftxui::text(value::encodeUtf8(imports.spinnerGlyph()))|
        ftxui::color(imports.running)|ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2);
    return ftxui::text(value::encodeUtf8(card.error?std::string("✗"):imports.okGlyph))|
      ftxui::color(card.error?imports.error:imports.success)|
      ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2);
  }
  if(!card.active)return ftxui::text("  ");
  if(imports.blackCircle.empty()||!imports.isBlinking)
    throw std::logic_error("Collapsed indicator glyph and blink provider required");
  const bool blinking=imports.isBlinking(card.shouldAnimate);
  const auto glyph=card.shouldAnimate&&!blinking&&!card.error?" ":
    imports.blackCircle;
  auto dot=ftxui::text(value::encodeUtf8(glyph));
  dot=dot|ftxui::color(card.error?imports.error:imports.subtle)|ftxui::dim;
  return dot|ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2);
}
std::string stringField(const Json& value,const char* key){
  if(!value.is_object())return {};
  const auto it=value.find(key);
  return it!=value.end()&&it->is_string()?it->get<std::string>():"";
}
double numberField(const Json& value,const char* key){
  if(!value.is_object())return 0;
  const auto it=value.find(key);
  return it!=value.end()&&it->is_number()?it->get<double>():0;
}
ftxui::Element dimText(const std::string& text,ftxui::Color subtle){
  return ftxui::text(value::encodeUtf8(text))|ftxui::color(subtle)|ftxui::dim;
}
ftxui::Element verboseView(const CollapsedCard& card,
                           const CollapsedProjectionImports& imports){
  if(!imports.renderVerboseGadgetUse)
    throw std::logic_error("Verbose collapsed Gadget renderer required");
  std::vector<ftxui::Element> rows;
  for(const auto& child:card.verboseGadgetUses)
    rows.push_back(imports.renderVerboseGadgetUse(child));
  if(card.hookInfos.is_array()&&!card.hookInfos.empty()){
    if(!imports.formatSecondsShort)
      throw std::logic_error("Hook duration formatter required");
    const double total=numberField(card.sourceMessage,"hookTotalMs");
    const double count=numberField(card.sourceMessage,"hookCount");
    rows.push_back(dimText("  ⎿  Ran "+value::numberToString(count)+
      " PreToolUse "+(count==1?"hook":"hooks")+" ("+
      imports.formatSecondsShort(total)+")",imports.subtle));
    for(const auto& info:card.hookInfos){
      rows.push_back(dimText("     ⎿ "+stringField(info,"command")+" ("+
        imports.formatSecondsShort(numberField(info,"durationMs"))+")",
        imports.subtle));
    }
  }
  for(const auto& memory:card.relevantMemories){
    if(!imports.basename||!imports.renderAnsi)
      throw std::logic_error("Memory basename and ANSI renderer required");
    rows.push_back(ftxui::vbox({dimText("  ⎿  Recalled "+
      imports.basename(stringField(memory,"path")),imports.subtle),
      ftxui::hbox({ftxui::text("     "),
        imports.renderAnsi(stringField(memory,"content"))})}));
  }
  return ftxui::vbox(std::move(rows));
}
}

std::optional<ftxui::Element> projectCollapsedReadSearch(
    const CollapsedCard& card,const CollapsedProjectionImports& imports){
  if(!card.visible)return {};
  ftxui::Element body;
  if(card.verbose)body=verboseView(card,imports);
  else{
    std::vector<ftxui::Element> spans;
    for(const auto& span:card.summary){
      auto value=ftxui::text(value::encodeUtf8(span.text));
      if(span.bold)value=value|ftxui::bold;
      if(span.bold&&imports.spinnerGlyph)value=value|ftxui::color(imports.text);
      if(span.hyperlink)
        value=value|ftxui::underlined|ftxui::hyperlink(*span.hyperlink);
      spans.push_back(std::move(value));
    }
    if(card.active)spans.push_back(ftxui::text("…"));
    spans.push_back(ftxui::text(" "));
    if(!card.expandHint.empty()){
      auto hint=ftxui::text(value::encodeUtf8(card.expandHint));
      // The shortcut reads as a key; the dimmed summary keeps it quiet.
      spans.push_back(imports.spinnerGlyph?hint|ftxui::dim:hint);
    }
    auto summary=ftxui::hbox({indicator(card,imports),
      ftxui::hflow(std::move(spans))|
      ftxui::color(imports.subtle)});
    std::vector<ftxui::Element> rows;
    rows.push_back(std::move(summary));
    for(const auto& hint:card.hintLines){
      rows.push_back(ftxui::hbox({dimText("  ⎿  ",imports.subtle)|
        ftxui::size(ftxui::WIDTH,ftxui::EQUAL,5),
        ftxui::paragraph(value::encodeUtf8(hint))|
        ftxui::color(imports.subtle)|ftxui::dim}));
    }
    if(card.hookSummary)rows.push_back(dimText(
      "  ⎿  "+*card.hookSummary,imports.subtle));
    body=ftxui::vbox(std::move(rows));
  }
  if(!card.selectedBackground.is_null()){
    if(!imports.resolveColor)
      throw std::logic_error("Collapsed selected-background resolver required");
    body=body|ftxui::bgcolor(imports.resolveColor(card.selectedBackground));
  }
  if(!card.verbose)body=ftxui::vbox({ftxui::text(""),std::move(body)});
  return body;
}
}
