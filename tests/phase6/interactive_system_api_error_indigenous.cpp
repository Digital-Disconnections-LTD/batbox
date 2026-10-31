#include "interactive/cards_system/SystemAPIErrorCard.hpp"
#include "interactive/cards_system/SystemAPIErrorProjection.hpp"
#include "interactive/cards_system/SystemAPIErrorTicker.hpp"
#include "retry/Retry.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_system;
using batbox::Json;

int main() {
  batbox::retry::ErrorRecord error;
  error.kind = batbox::retry::ErrorRecord::Kind::Api;
  error.message = "Request failed";
  const Json message{{"type","system"},{"subtype","api_error"},
      {"retryAttempt",4},{"retryInMs",1000},{"maxRetries",8},
      {"error",batbox::retry::errorToJson(error)}};
  const auto input = apiErrorInputFromMessage(message, false, std::nullopt);
  assert(input.formattedError == "Request failed");
  assert(input.retryAttempt == 4 && input.retryInMs == 1000);

  batbox::event_loop::Loop loop;
  int changes = 0;
  APIErrorTicker ticker(loop, [&](const APIErrorCard& card) {
    ++changes;
    assert(card.visible);
  });
  const auto initial = ticker.setInput(input);
  assert(initial.intervalActive && changes == 1);
  const APIErrorPalette palette{ftxui::Color::RedLight,
                                ftxui::Color::GrayDark};
  const auto element = projectSystemAPIError(initial, palette,
      [] {return ftxui::text("(expand)");});
  assert(element);
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                      ftxui::Dimension::Fixed(3));
  ftxui::Render(screen, *element);
  assert(screen.CellAt(2,0).character == "⎿");
  assert(screen.CellAt(5,0).character == "R");
  assert(screen.CellAt(5,1).character == "R");

  loop.run();
  assert(changes == 2);
  assert(!ticker.current().intervalActive);
  assert(ticker.current().retryText.starts_with("Retrying in 0 seconds"));
  const auto hidden = APIErrorState{}.render({3,1000,8,"bad",false,{}});
  assert(!projectSystemAPIError(hidden, palette, {}));
  std::cout << "4 native system API error binding/timer/projection checks pass\n";
}
