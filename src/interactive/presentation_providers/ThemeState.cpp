#include "interactive/presentation_providers/ThemeState.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::presentation_providers {

ThemeState::ThemeState(ThemeStateServices services,
    std::optional<std::string> initialState):services_(std::move(services)){
  if(!services_.loadSetting||!services_.saveSetting||
     !services_.getSystemTheme||!services_.onChanged)
    throw std::invalid_argument("Theme state imports required");
  setting_=initialState.value_or(services_.loadSetting());
  systemTheme_=setting_=="auto"?services_.getSystemTheme():"dark";
}
std::string ThemeState::currentTheme()const{
  return resolveThemeName(preview_.value_or(setting_),systemTheme_);
}
void ThemeState::setSetting(std::string setting){
  setting_=std::move(setting);
  preview_.reset();
  if(setting_=="auto")systemTheme_=services_.getSystemTheme();
  services_.saveSetting(setting_);
  services_.onChanged();
}
void ThemeState::setPreview(std::string setting){
  preview_=std::move(setting);
  if(*preview_=="auto")systemTheme_=services_.getSystemTheme();
  services_.onChanged();
}
void ThemeState::savePreview(){
  if(!preview_)return;
  setting_=*preview_;
  preview_.reset();
  services_.saveSetting(setting_);
  services_.onChanged();
}
void ThemeState::cancelPreview(){
  if(!preview_)return;
  preview_.reset();
  services_.onChanged();
}
void ThemeState::systemThemeChanged(std::string theme){
  systemTheme_=std::move(theme);
  if(preview_.value_or(setting_)=="auto")services_.onChanged();
}
} // namespace batbox::interactive::presentation_providers
