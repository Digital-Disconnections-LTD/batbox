#pragma once

#include "interactive/MessageLookups.hpp"
#include "gadgets/Gadget.hpp"
#include <optional>
#include <string>

namespace batbox::interactive::cards_gadget_result {

enum class GadgetResultKind { Hidden, Canceled, Rejected, Error, Success };

struct GadgetResultProtocol {
  std::string cancelMessage;
  std::string rejectMessage;
  std::string interruptForGadgetUse;
};

struct GadgetResultCard {
  GadgetResultKind kind = GadgetResultKind::Hidden;
  const gadgets::Gadget* gadget = nullptr;
  std::optional<GadgetBlockRef> gadgetUse;
  std::string gadgetUseID;
};

GadgetResultCard classifyGadgetResult(const Json& param,
                                  const MessageLookups& lookups,
                                  const gadgets::Gadgets& gadgets,
                                  const GadgetResultProtocol& protocol);
const char* gadgetResultKindName(GadgetResultKind kind);

} // namespace batbox::interactive::cards_gadget_result
