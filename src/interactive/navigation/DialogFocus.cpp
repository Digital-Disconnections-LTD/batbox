#include "interactive/navigation/DialogFocus.hpp"

namespace batbox::interactive::navigation {
Dialog DialogFocus::observe(const DialogInputs& input,double nowMs){
  const auto next=focusedInputDialog(input);
  const bool was=current_&&*current_=="tool-permission";
  const bool now=next&&*next=="tool-permission";
  current_=next;
  if(input.isLoading)loading_.observePermissionPause(now,nowMs);
  if(initialized_&&was!=now&&repinScroll_)repinScroll_();
  initialized_=true;
  return current_;
}
} // namespace batbox::interactive::navigation
