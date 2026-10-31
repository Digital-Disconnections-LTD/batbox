#include "interactive/cards_system/SystemTextProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_system;
using batbox::Json;

namespace {
ftxui::Screen draw(const ftxui::Element& element) {
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                    ftxui::Dimension::Fixed(3));
  ftxui::Render(screen,element);
  return screen;
}
}

int main() {
  SystemProjectionImports imports;
  imports.palette={ftxui::Color::White,ftxui::Color::GrayDark,
                   ftxui::Color::RedLight,ftxui::Color::Blue};
  imports.referenceMark="※";
  imports.errorDot="●";
  imports.eventMark="✻";
  int specialCalls=0;
  imports.renderSpecial=[&](SystemTextKind kind,const Json&,
      const SystemTextCard&,bool) -> std::optional<ftxui::Element> {
    assert(kind==SystemTextKind::APIError);
    ++specialCalls;
    return ftxui::text("API");
  };
  const auto away=projectSystemTextMessage(
    Json{{"type","system"},{"subtype","away_summary"},
         {"level","info"},{"content","finished"}},
    true,false,false,imports);
  assert(away);
  auto screen=draw(*away);
  assert(screen.CellAt(0,1).character=="※");
  assert(screen.CellAt(2,1).character=="f");
  const auto killed=projectSystemTextMessage(
    Json{{"type","system"},{"subtype","agents_killed"},
         {"level","info"}},false,false,false,imports);
  assert(killed);
  screen=draw(*killed);
  assert(screen.CellAt(0,0).character=="●");
  assert(screen.CellAt(2,0).character=="A");
  const auto scheduled=projectSystemTextMessage(
    Json{{"type","system"},{"subtype","scheduled_task_fire"},
         {"level","info"},{"content","build"}},
    false,false,false,imports);
  assert(scheduled);
  screen=draw(*scheduled);
  assert(screen.CellAt(0,0).character=="✻");
  const auto permission=projectSystemTextMessage(
    Json{{"type","system"},{"subtype","permission_retry"},
         {"level","info"},{"commands",Json::array({"pwd","git status"})}},
    false,false,false,imports);
  assert(permission);
  screen=draw(*permission);
  assert(screen.CellAt(2,0).character=="A");
  assert(screen.CellAt(10,0).character=="p");
  const auto quiet=projectSystemTextMessage(
    Json{{"type","system"},{"subtype","other"},
         {"level","info"},{"content","quiet"}},
    false,false,false,imports);
  assert(!quiet);
  const auto api=projectSystemTextMessage(
    Json{{"type","system"},{"subtype","api_error"},
         {"level","error"}},false,false,false,imports);
  assert(api && specialCalls==1);
  std::cout<<"6 native system-text routing/projection checks pass\n";
}
