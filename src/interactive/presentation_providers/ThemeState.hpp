#pragma once

#include "interactive/presentation_providers/SystemTheme.hpp"
#include "interactive/presentation_providers/ThemeData.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::presentation_providers {

struct ThemeStateServices {
  std::function<std::string()> loadSetting;
  std::function<void(const std::string&)> saveSetting;
  std::function<std::string()> getSystemTheme;
  std::function<void()> onChanged;
};

class ThemeState {
public:
  ThemeState(ThemeStateServices services,
    std::optional<std::string> initialState={});
  const std::string& setting()const{return setting_;}
  const std::optional<std::string>& preview()const{return preview_;}
  std::string currentTheme()const;
  void setSetting(std::string setting);
  void setPreview(std::string setting);
  void savePreview();
  void cancelPreview();
  void systemThemeChanged(std::string theme);
private:
  ThemeStateServices services_;
  std::string setting_,systemTheme_;
  std::optional<std::string> preview_;
};

} // namespace batbox::interactive::presentation_providers
