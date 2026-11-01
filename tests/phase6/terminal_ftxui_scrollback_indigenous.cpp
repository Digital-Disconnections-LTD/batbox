#include "terminal_ftxui/ScrollbackProjection.hpp"
#include "interactive/TranscriptRows.hpp"
#include <cassert>
#include <iostream>

using batbox::interactive::PresentationState;
using Kind = batbox::interactive::TranscriptRow::Kind;
namespace fx = batbox::terminal_ftxui;

int main() {
  PresentationState state;
  state.input.poetry = "> ";
  state.input.focused = true;
  state.transcriptRows = {
    {Kind::User, "u", "first", false, "1:0"},
    {Kind::Assistant, "a", "second", false, "2:0"},
    {Kind::Assistant, "b", "third", false, "3:0"},
    {Kind::Gadget, "c", "fourth", false, "4:0"},
    {Kind::GadgetResult, "d", "fifth", false, "5:0"}
  };
  fx::ScrollbackCursor cursor;
  const ftxui::Dimensions dimensions{12, 4};
  auto rows = fx::advanceInlineScrollback(state, dimensions, cursor, true);
  assert(rows.size() == 2);
  assert(rows[0].text == "\x1b[" + fx::colorFor(Kind::User).Print(false) + "m❯ first\x1b[39m");
  assert(rows[1].text == "\x1b[" + fx::colorFor(Kind::Assistant).Print(false) + "m  second\x1b[39m");
  assert(rows[0].id != rows[1].id);
  assert(fx::layoutPresentation(state, 12, 4, &cursor.committed).transcriptRowCount == 3);
  assert(fx::advanceInlineScrollback(state, dimensions, cursor, true).empty());
  state.transcriptRows.push_back({Kind::Assistant, "e", "sixth", false, "6:0"});
  rows = fx::advanceInlineScrollback(state, dimensions, cursor, true);
  assert(rows.size() == 1 && rows[0].text == "\x1b[" +
    fx::colorFor(Kind::Assistant).Print(false) + "m  third\x1b[39m");
  assert(fx::layoutPresentation(state, 12, 4, &cursor.committed).transcriptRowCount == 3);
  state.viewport.follow = false;
  state.transcriptRows.push_back({Kind::Progress, "p", "working", true, "7:0"});
  assert(fx::advanceInlineScrollback(state, dimensions, cursor, true).empty());
  state.viewport.follow = true;
  rows = fx::advanceInlineScrollback(state, dimensions, cursor, false);
  assert(rows.empty());
  auto full = fx::renderPresentationWithScrollback(state, dimensions, cursor, false);
  auto inlineView = fx::renderPresentationWithScrollback(state, dimensions, cursor, true);
  assert(full && inlineView);
  PresentationState ownership;
  auto first = std::make_shared<batbox::Json>(batbox::Json{
    {"type", "user"}, {"uuid", "same"},
    {"message", batbox::Json{{"content", "alpha"}}}});
  ownership.messages.push_back(first);
  batbox::interactive::rebuildTranscriptRows(ownership);
  const auto firstId = ownership.transcriptRows.front().scrollbackId;
  batbox::interactive::rebuildTranscriptRows(ownership);
  assert(ownership.transcriptRows.front().scrollbackId == firstId);
  ownership.messages.clear();
  first.reset();
  ownership.messages.push_back(std::make_shared<batbox::Json>(batbox::Json{
    {"type", "user"}, {"uuid", "same"},
    {"message", batbox::Json{{"content", "beta"}}}}));
  batbox::interactive::rebuildTranscriptRows(ownership);
  assert(ownership.transcriptRows.front().scrollbackId != firstId);
  std::cout << "8 native inline scrollback projection cases pass\n";
}
