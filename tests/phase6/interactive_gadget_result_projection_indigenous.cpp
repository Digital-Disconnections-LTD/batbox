#include "interactive/cards_gadget_result/GadgetResultProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive;
using namespace batbox::interactive::cards_gadget_result;
using batbox::Json;

namespace {
ftxui::Screen draw(const ftxui::Element& element) {
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(50),
                                    ftxui::Dimension::Fixed(2));
  ftxui::Render(screen,element);
  return screen;
}
}

int main() {
  auto owner=std::make_shared<Json>(Json{{"type","assistant"},
    {"message",Json{{"content",Json::array({
      Json{{"type","tool_use"},{"id","tool"},
           {"name","TestTool"},{"input",Json::object()}}
    })}}}});
  MessageLookups lookups;
  lookups.gadgetUseByGadgetUseID["tool"]={owner,0};
  batbox::gadgets::Gadget gadget;
  gadget.name="TestTool";
  batbox::gadgets::Gadgets gadgets{&gadget};
  GadgetResultProjectionImports imports;
  imports.resultProtocol={"CANCEL","REJECT","INTERRUPT"};
  imports.errorProtocol={"INTERRUPT","PLAN:","REJECT-REASON:",
                         "CLASSIFIER:"};
  imports.palette={{ftxui::Color::GrayDark,ftxui::Color::White,
    ftxui::Color::RedLight,ftxui::Color::YellowLight},
    ftxui::Color::GrayDark};
  imports.columns=50;
  imports.shortcutDisplay=[] {return "ctrl+o";};
  int successCalls=0,rejectCalls=0,gadgetErrorCalls=0;
  imports.renderSuccess=[&](const GadgetResultCard& card,const Json&) {
    ++successCalls;
    assert(card.gadget==&gadget&&card.gadgetUseID=="tool");
    return std::optional<ftxui::Element>{ftxui::text("SUCCESS")};
  };
  imports.renderRejected=[&](const GadgetResultCard&,const Json&) {
    ++rejectCalls;
    return std::optional<ftxui::Element>{ftxui::text("REJECTED")};
  };
  imports.renderRejectedPlan=[](const std::string& plan) {
    return std::optional<ftxui::Element>{ftxui::text("PLAN:"+plan)};
  };
  imports.renderGadgetError=[&](const GadgetResultCard&,const Json&,
                                const Json&) {
    ++gadgetErrorCalls;
    return std::optional<ftxui::Element>{ftxui::text("TOOL ERROR")};
  };
  auto param=[](const Json& content,bool error=false) {
    return Json{{"type","tool_result"},{"tool_use_id","tool"},
                {"content",content},{"is_error",error}};
  };
  const Json progress=Json::array();
  assert(!projectGadgetResultBlock(param("plain"),progress,MessageLookups{},
                                 gadgets,false,imports));
  const auto success=projectGadgetResultBlock(param("done"),progress,lookups,
                                             gadgets,false,imports);
  assert(success&&successCalls==1);
  assert(draw(*success).CellAt(0,0).character=="S");
  const auto canceled=projectGadgetResultBlock(param("CANCEL now",true),progress,
                                              lookups,gadgets,false,imports);
  assert(canceled);
  assert(draw(*canceled).CellAt(5,0).character=="I");
  const auto rejected=projectGadgetResultBlock(param("REJECT no"),progress,
                                              lookups,gadgets,false,imports);
  assert(rejected&&rejectCalls==1);
  assert(draw(*rejected).CellAt(0,0).character=="R");
  const auto interrupted=projectGadgetResultBlock(param("INTERRUPT",true),
      progress,lookups,gadgets,false,imports);
  assert(interrupted&&rejectCalls==2);
  assert(draw(*interrupted).CellAt(0,0).character=="R");
  const auto embedded=projectGadgetResultBlock(param("before INTERRUPT",true),
      progress,lookups,gadgets,false,imports);
  assert(embedded);
  assert(draw(*embedded).CellAt(5,0).character=="I");
  const auto fallback=projectGadgetResultBlock(param("bad",true),progress,
                                              lookups,gadgets,false,imports);
  assert(fallback&&gadgetErrorCalls==0);
  assert(draw(*fallback).CellAt(5,0).character=="E");
  gadget.renderGadgetUseErrorMessage=[](const Json&,const Json&) {
    return Json("custom");
  };
  const auto custom=projectGadgetResultBlock(param("bad",true),progress,
                                            lookups,gadgets,false,imports);
  assert(custom&&gadgetErrorCalls==1);
  assert(draw(*custom).CellAt(0,0).character=="T");
  std::cout<<"8 native tool-result routing/projection checks pass\n";
}
