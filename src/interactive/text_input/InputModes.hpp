#pragma once
#include <string>

namespace batbox::interactive::text_input {
std::string prependModeCharacterToInput(const std::string& input,const std::string& mode);
std::string getModeFromInput(const std::string& input);
std::string getValueFromInput(const std::string& input);
bool isInputModeCharacter(const std::string& input);
} // namespace batbox::interactive::text_input
