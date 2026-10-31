#include "interactive/text_input/InputModes.hpp"

namespace batbox::interactive::text_input {
std::string prependModeCharacterToInput(const std::string& input,const std::string& mode){
  return mode=="bash"?"!"+input:input;
}
std::string getModeFromInput(const std::string& input){
  return input.starts_with('!')?"bash":"prompt";
}
std::string getValueFromInput(const std::string& input){
  return getModeFromInput(input)=="prompt"?input:input.substr(1);
}
bool isInputModeCharacter(const std::string& input){return input=="!";}
} // namespace batbox::interactive::text_input
