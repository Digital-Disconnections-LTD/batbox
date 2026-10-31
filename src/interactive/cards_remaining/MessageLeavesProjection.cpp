#include "interactive/cards_remaining/MessageLeavesProjection.hpp"
#include "runtime/Encoding.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::cards_remaining {
namespace {
ftxui::Element response(ftxui::Element body,ftxui::Color subtle){
  return ftxui::hbox({ftxui::text("  ⎿  ")|
    ftxui::color(subtle)|ftxui::dim,std::move(body)});
}
ftxui::Element dim(const std::string& text,ftxui::Color subtle){
  return ftxui::text(value::encodeUtf8(text))|ftxui::color(subtle)|ftxui::dim;
}
ftxui::Element dot(const LeafPalette& palette,bool resolved,bool error,
                   bool animate){
  if(palette.blackCircle.empty()||!palette.isBlinking)
    throw std::logic_error("Advisor indicator imports required");
  const bool blinking=palette.isBlinking(animate);
  const auto glyph=animate&&!blinking&&!error&&!resolved?" ":
    palette.blackCircle;
  auto element=ftxui::text(value::encodeUtf8(glyph));
  if(!resolved)element=element|ftxui::color(palette.subtle)|ftxui::dim;
  else element=element|ftxui::color(error?palette.error:palette.success);
  return element|ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2);
}
}
ftxui::Element projectUserImage(const ImageCard& card,
                                const LeafPalette& palette){
  auto label=ftxui::text(value::encodeUtf8(card.label));
  if(card.fileURL)label=label|ftxui::hyperlink(*card.fileURL);
  return card.addMargin?ftxui::vbox({ftxui::text(""),std::move(label)}):
    response(std::move(label),palette.subtle);
}
ftxui::Element projectCompactSummary(const CompactCard& card,
                                     const LeafPalette& palette){
  if(palette.blackCircle.empty())
    throw std::logic_error("Compact summary indicator glyph required");
  std::vector<ftxui::Element> detail;
  auto title=ftxui::text(value::encodeUtf8(card.title))|ftxui::bold;
  if(!card.metadata&&!card.transcriptMode&&!card.shortcut.empty())
    title=ftxui::hbox({std::move(title),dim(" "+card.shortcut,palette.subtle)});
  detail.push_back(std::move(title));
  if(card.transcriptMode){
    if(!card.body.empty())detail.push_back(response(
      ftxui::paragraph(value::encodeUtf8(card.body)),palette.subtle));
  }else if(card.metadata){
    std::vector<ftxui::Element> metadata;
    metadata.push_back(dim(card.body,palette.subtle));
    if(!card.context.empty())metadata.push_back(dim(card.context,palette.subtle));
    metadata.push_back(dim(card.shortcut,palette.subtle));
    detail.push_back(response(ftxui::vbox(std::move(metadata)),palette.subtle));
  }
  auto body=ftxui::hbox({ftxui::text(value::encodeUtf8(palette.blackCircle))|
    ftxui::color(palette.text)|ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2),
    ftxui::vbox(std::move(detail))});
  return ftxui::vbox({ftxui::text(""),std::move(body)});
}
ftxui::Element projectCompactBoundary(const std::string& text,
                                      const LeafPalette& palette){
  return ftxui::vbox({ftxui::text(""),dim(text,palette.subtle),
                     ftxui::text("")});
}
std::optional<ftxui::Element> projectAdvisor(const AdvisorCard& card,
                                             const LeafPalette& palette){
  if(card.kind==AdvisorCard::Kind::Hidden)return {};
  if(card.kind==AdvisorCard::Kind::GadgetUse){
    auto row=ftxui::hbox({dot(palette,card.resolved,card.errored,
      card.animate&&!card.resolved),
      ftxui::text("Advising")|ftxui::bold,
      dim(card.modelName.empty()?"":" using "+card.modelName,palette.subtle),
      dim(card.input.empty()?"":" · "+card.input,palette.subtle)});
    return card.addMargin?ftxui::vbox({ftxui::text(""),std::move(row)}):row;
  }
  if(card.kind==AdvisorCard::Kind::Error)
    return response(ftxui::text(value::encodeUtf8(card.text))|
      ftxui::color(palette.error),palette.subtle);
  if(card.kind==AdvisorCard::Kind::Redacted||!card.verbose){
    if(palette.successTick.empty())
      throw std::logic_error("Advisor success glyph required");
    return response(dim(palette.successTick+" "+card.text,palette.subtle),
                    palette.subtle);
  }
  return response(dim(card.text,palette.subtle),palette.subtle);
}

std::optional<ftxui::Element> projectPartnerHeader(
    const PartnerHeaderCard& card,
    const std::function<ftxui::Color(const std::string&)>& toColor,
    const LeafPalette& palette){
  if(!card.visible)return {};
  if(!toColor)throw std::logic_error("Partner color resolver required");
  auto line=ftxui::hbox({ftxui::text("Viewing "),
    ftxui::text(value::encodeUtf8("@"+card.name))|
      ftxui::color(toColor(card.color))|ftxui::bold,
    dim(" · "+card.exitHint,palette.subtle)});
  return ftxui::vbox({std::move(line),dim(card.poetry,palette.subtle),
                      ftxui::text("")});
}
}
