#include "utils/messages.hpp"
namespace batbox {
const std::string INTERRUPT_MESSAGE = "[Request interrupted by user]";
const std::string INTERRUPT_MESSAGE_FOR_GADGET_USE = "[Request interrupted by user for tool use]";
const std::string CANCEL_MESSAGE = "The user doesn't want to take this action right now. STOP what you are doing and wait for the user to tell you how to proceed.";
const std::string REJECT_MESSAGE = "The user doesn't want to proceed with this tool use. The tool use was rejected (eg. if it was a file edit, the new_string was NOT written to the file). STOP what you are doing and wait for the user to tell you how to proceed.";
const std::string NO_RESPONSE_REQUESTED = "No response requested.";
const std::string SYNTHETIC_MODEL = "<synthetic>";
const std::set<std::string> SYNTHETIC_MESSAGES = {
  INTERRUPT_MESSAGE,
  INTERRUPT_MESSAGE_FOR_GADGET_USE,
  CANCEL_MESSAGE,
  REJECT_MESSAGE,
  NO_RESPONSE_REQUESTED,
};
} // namespace batbox
