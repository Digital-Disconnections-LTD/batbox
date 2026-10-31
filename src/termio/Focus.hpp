#pragma once
#include "bootstrap/Signal.hpp"
#include <string>
namespace batbox::termio {
class FocusModule {
public:
  using Callback=std::shared_ptr<bootstrap::Signal<>::Listener>;
  void setTerminalFocused(bool focused);
  bool getTerminalFocused()const;
  const std::string& getTerminalFocusState()const;
  std::function<void()> subscribeTerminalFocus(Callback callback);
  void resetTerminalFocusState();
private:
  std::string focusState_="unknown";
  bootstrap::Signal<> resolvers_,subscribers_;
};
}
