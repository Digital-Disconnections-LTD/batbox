#pragma once
#include "interactive/PresentationState.hpp"
#include "interactive/text_input/MeasuredText.hpp"
#include <ftxui/dom/node.hpp>

namespace batbox::interactive::conversation_shell {
double poetryEditorColumns(const InputState& input,int columns,
  const text_input::MetricsServices& metrics);
ftxui::Element poetryViewport(const InputState& input,int columns,int maxRows,
  const text_input::MetricsServices& metrics,bool cursorVisible);
}
