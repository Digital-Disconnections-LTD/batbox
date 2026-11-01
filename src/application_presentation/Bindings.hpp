#pragma once
#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/presentation_providers/Chrome.hpp"
#include "interactive/markdown/IndigenousRenderer.hpp"
#include "messages/Messages.hpp"
#include "text_width/IndigenousLayout.hpp"
#include <memory>

namespace batbox::application_presentation {
// The owner retains this bundle for as long as the shell can render. All selectors
// read live application state at paint time; gadgets are borrowed from the roster.
struct Inputs {
  interactive::presentation_providers::ThemeState* theme=nullptr;
  interactive::presentation_providers::ShortcutProvider shortcuts;
  messages::Services* messageServices=nullptr;
  std::function<gadgets::Gadgets()> gadgets;
  std::function<Json()> commands;
  std::function<std::string()> modelLabel,workingDirectory,permissionMode,statusText;
  std::function<bool(const std::string&)> feature;
  std::function<void(const std::string&)> log;
  std::function<std::optional<std::string>(int)> storedImagePath;
  std::function<std::string(const std::string&)> displayPath;
  // Optional status-row context.
  std::function<std::string()> gitBranch,homeDirectory;
  std::function<std::optional<double>()> contextUsage;
  // Paint clock in milliseconds for spinners; a steady clock by default.
  std::function<double()> nowMs;
  bool colors=true,hyperlinks=false;
};
class Bundle final:public std::enable_shared_from_this<Bundle> {
public:
  static std::shared_ptr<Bundle> bind(interactive::conversation_shell::Imports& shell,Inputs inputs);
private:
  Bundle(interactive::conversation_shell::Imports& shell,Inputs inputs);
  interactive::MessageListSettings listSettings(const interactive::PresentationState&)const;
  interactive::message_list_view::SurfaceOptions surfaceOptions(const interactive::PresentationState&)const;
  interactive::message_view::ProjectionServices projectionServices()const;
  interactive::message_view::RetainedViewServices retainedServices()const;
  Inputs inputs_;
  interactive::PresentationState* state_=nullptr;
  std::shared_ptr<intl::Stash> intl_;
  std::shared_ptr<text_width::StringWidth> width_;
  text_width::WrapAnsiRef wrap_;
  std::shared_ptr<interactive::markdown::Renderer> markdown_;
  interactive::markdown::StreamingState streaming_;
  struct MarkdownPalette;
  std::shared_ptr<MarkdownPalette> markdownPalette_;
  void refreshMarkdownPalette()const;
  std::string spinnerGlyph()const;
  std::string expandKey()const;
};
} // namespace batbox::application_presentation
