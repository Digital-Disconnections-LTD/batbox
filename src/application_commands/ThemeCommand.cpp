#include "application_commands/ThemeCommand.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
namespace ip=interactive::presentation_providers;
using ThemeStatePtr=std::shared_ptr<ip::ThemeState>;

std::string label(const std::string& name){
  for(const auto& choice:ip::selectableThemes())if(choice.name==name)return choice.label;
  return name;
}
std::string names(){
  std::string text;
  for(const auto& choice:ip::selectableThemes()){
    if(!text.empty())text+=", ";
    text+=choice.name;
  }
  return text;
}
interactive::CommandCompletion completion(std::string message){
  interactive::CommandCompletion result;
  result.result=std::move(message);
  result.options.display=interactive::DoneOptions::Display::System;
  return result;
}

class ThemePicker final:public std::enable_shared_from_this<ThemePicker> {
public:
  ThemePicker(ThemeStatePtr theme,interactive::CommandUi::Done done,
      interactive::CommandUi::Ready ready):theme_(std::move(theme)),
      done_(std::move(done)),ready_(std::move(ready)),choices_(ip::selectableThemes()){
    const auto current=ip::canonicalThemeName(theme_->setting());
    for(std::size_t i=0;i<choices_.size();++i)if(choices_[i].name==current)selected_=i;
  }
  void show(){
    interactive::UiRequest request;
    request.kind=interactive::UiRequest::Kind::Select;
    request.id="theme";request.title="Theme";
    request.body="The highlighted theme previews live behind this dialog.";
    request.hint="↑↓ preview · enter apply · esc keep current";
    const auto current=ip::canonicalThemeName(theme_->setting());
    for(const auto& choice:choices_){
      request.choices.push_back(choice.label);
      interactive::UiChoice info;
      info.detail=choice.description;
      if(choice.name=="auto")
        info.detail+=" (now "+label(ip::themeForSetting("auto",ip::activeColorDepth(),
          theme_->systemTheme()))+")";
      if(choice.name==current)info.badge="current";
      request.choiceInfo.push_back(std::move(info));
    }
    request.selectedIndex=selected_;
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      self->act(action);});
  }
private:
  void act(const interactive::UiAction& action){
    using Kind=interactive::UiAction::Kind;
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){
      theme_->cancelPreview();
      done_(completion("Theme unchanged: "+label(ip::canonicalThemeName(theme_->setting()))+"."));
      return;
    }
    if(action.kind==Kind::MoveSelection){
      selected_=std::min(action.index,choices_.size()-1);
      theme_->setPreview(choices_[selected_].name);
      show();return;
    }
    if(action.kind!=Kind::Activate)return;
    selected_=std::min(action.index,choices_.size()-1);
    const auto& chosen=choices_[selected_];
    theme_->setSetting(chosen.name);
    done_(completion("Theme set to "+chosen.label+"."));
  }
  ThemeStatePtr theme_;
  interactive::CommandUi::Done done_;
  interactive::CommandUi::Ready ready_;
  std::vector<ip::ThemeChoice> choices_;
  std::size_t selected_=0;
};
} // namespace

user_input::CommandEntry makeThemeCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::function<ThemeStatePtr()> theme,bool interactive){
  if(!router||!theme)throw std::invalid_argument("Theme command needs a router and theme state");
  Json descriptor={{"type","local-jsx"},{"name","theme"},
    {"description","Choose the color theme"},
    {"argumentHint","[auto|batbox-dark|batbox-light|batbox-ansi|monochrome]"},
    {"supportsNonInteractive",true}};
  return user_input::bindUiCommand(std::move(descriptor),std::move(router),
    [theme=std::move(theme),interactive](const std::string& args){
      return [theme,interactive,args](auto done,auto ready,auto){
        const auto state=theme();
        const auto argument=value::trim(args);
        if(!state){done(completion("Themes are available in the interactive terminal."));return;}
        if(!argument.empty()){
          const auto name=ip::canonicalThemeName(argument);
          if(!ip::isKnownTheme(name)){
            done(completion("Unknown theme '"+argument+"'. Choose one of: "+names()+"."));
            return;
          }
          state->setSetting(name);
          done(completion("Theme set to "+label(name)+"."));
          return;
        }
        if(!interactive){
          done(completion("Theme: "+label(ip::canonicalThemeName(state->setting()))+
            ". Choose one of: "+names()+"."));
          return;
        }
        std::make_shared<ThemePicker>(state,std::move(done),std::move(ready))->show();
      };
    });
}
} // namespace batbox::application_commands
