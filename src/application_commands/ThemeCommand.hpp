#pragma once
#include "interactive/presentation_providers/ThemeState.hpp"
#include "user_input/UiCommandRouter.hpp"
#include <functional>
#include <memory>

namespace batbox::application_commands {
// /theme opens a picker that previews each theme live as the selection
// moves; Enter saves the choice, Esc restores the current theme.
// "/theme <name>" applies a theme directly.
user_input::CommandEntry makeThemeCommand(
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::function<std::shared_ptr<interactive::presentation_providers::ThemeState>()> theme,
  bool interactive);
}
