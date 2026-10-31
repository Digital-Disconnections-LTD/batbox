#include "termio/Focus.hpp"
namespace batbox::termio {
void FocusModule::setTerminalFocused(bool focused){
  focusState_=focused?"focused":"blurred";
  subscribers_.emit();
  if(!focused){resolvers_.emit();resolvers_.clear();}
}
bool FocusModule::getTerminalFocused()const{return focusState_!="blurred";}
const std::string& FocusModule::getTerminalFocusState()const{return focusState_;}
std::function<void()> FocusModule::subscribeTerminalFocus(Callback callback){return subscribers_.subscribe(std::move(callback));}
void FocusModule::resetTerminalFocusState(){focusState_="unknown";subscribers_.emit();}
}
