#pragma once
#include "interactive/message_view/Projection.hpp"
#include <cassert>
namespace message_view_fixture {
inline batbox::interactive::message_view::ProjectionServices services(){
  using namespace batbox::interactive;
  message_view::ProjectionServices result;
  auto& user=result.user;
  user.protocol={"__NONE__","__INTERRUPT__","__INTERRUPT_TOOL__",
    "command-message","local-command-caveat","tick","teammate-message",
    "task-notification","GITHUB","FORK","INBOX","CHANNEL","CHANNELS",
    [](const std::string&){return false;},[]{return false;}};
  user.userPalette={ftxui::Color::YellowLight,ftxui::Color::White,
    ftxui::Color::GrayDark,ftxui::Color::Blue,ftxui::Color::Cyan};
  user.poetryPalette={ftxui::Color::Cyan,ftxui::Color::YellowLight,
    [](const std::string&){return ftxui::Color::White;}};
  user.poetry.logError=[](const std::string&){assert(false);};
  user.highlight.pointerGlyph=">";
  user.bashResult.outputPalette={ftxui::Color::GrayDark,ftxui::Color::White,
    ftxui::Color::RedLight,ftxui::Color::YellowLight};
  user.bashResult.dimColor=ftxui::Color::GrayDark;
  user.bashResult.outputLine.stringWidth=[](const std::string& text){return static_cast<double>(text.size());};
  user.bashResult.outputLine.sliceAnsi=[](const std::string& text,double start,double end){return text.substr(start,end-start);};
  user.bashResult.outputLine.dim=[](const std::string& text){return text;};
  user.bashResult.outputLine.expandHint=[]{return "(expand)";};
  auto& assistant=result.assistant;
  assistant.protocol={"__NONE__","__NO_RESPONSE__","Prompt is too long",
    "__CREDIT__","low credit","__KEY__","__EXTERNAL_KEY__","__ORG__",
    "__ORG_AUTH__","__TOKEN__","__TIMEOUT__","__MODEL_OFF__","__ABORT__",
    "API Error",{"You've hit your"}};
  assistant.textImports.contextUpgradeHint=[]{return std::string{};};
  assistant.textImports.isKeychainLocked=[]{return false;};
  assistant.textImports.fallbackModelName=[]{return "fallback";};
  assistant.textImports.startsWithApiErrorPrefix=[](const std::string& text){return text.starts_with("API Error");};
  assistant.projection.palette={ftxui::Color::White,ftxui::Color::GrayDark,
    ftxui::Color::RedLight,ftxui::Color::YellowLight,ftxui::Color::Blue};
  assistant.projection.dotGlyph="*";
  assistant.projection.renderMarkdown=[](const std::string& text,bool){return ftxui::text(text);};
  assistant.projection.renderExpandHint=[]{return ftxui::text("(expand)");};
  result.system.palette={ftxui::Color::White,ftxui::Color::GrayDark,
    ftxui::Color::RedLight,ftxui::Color::Blue};
  result.system.referenceMark="ref";result.system.errorDot="!";result.system.eventMark="event";
  result.metadata={[](const std::string&){return std::string("04:05 PM");},
    [](const std::string& text){return static_cast<int>(text.size());}};
  result.gadgetResult=[](const message_view::MessagePlan& message,const message_view::BlockPlan& block,
      const MessageLookups&,const batbox::gadgets::Gadgets&){
    assert(block.width==message.options.columns-5);
    cards_gadget_result::GadgetResultProjectionImports imports;
    imports.resultProtocol={"CANCEL","REJECT","INTERRUPT"};
    imports.errorProtocol={"INTERRUPT","PLAN:","REJECT-REASON:","CLASSIFIER:"};
    imports.palette={{ftxui::Color::GrayDark,ftxui::Color::White,
      ftxui::Color::RedLight,ftxui::Color::YellowLight},ftxui::Color::GrayDark};
    imports.shortcutDisplay=[]{return "ctrl+o";};
    return imports;
  };
  return result;
}
} // namespace message_view_fixture
