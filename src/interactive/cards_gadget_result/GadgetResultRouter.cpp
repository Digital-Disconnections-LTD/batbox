#include "interactive/cards_gadget_result/GadgetResultRouter.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_result {
namespace {
std::string string(const Json& object, const char* key) {
  const auto found = object.find(key);
  return found != object.end() && found->is_string() ?
      found->get<std::string>() : std::string{};
}
}

GadgetResultCard classifyGadgetResult(const Json& param,
                                  const MessageLookups& lookups,
                                  const gadgets::Gadgets& gadgets,
                                  const GadgetResultProtocol& protocol) {
  if (protocol.cancelMessage.empty() || protocol.rejectMessage.empty() ||
      protocol.interruptForGadgetUse.empty())
    throw std::invalid_argument("Gadget result protocol required");
  const auto gadgetUseID = string(param,"tool_use_id");
  const auto found = lookups.gadgetUseByGadgetUseID.find(gadgetUseID);
  if (found == lookups.gadgetUseByGadgetUseID.end()) return {};
  const auto& gadgetUse = found->second.value();
  const auto* gadget = gadgets::findGadgetByName(gadgets,string(gadgetUse,"name"));
  if (!gadget) return {};
  GadgetResultCard card{GadgetResultKind::Success,gadget,found->second,
                      string(gadgetUse,"id")};
  const auto content = param.find("content");
  if (content != param.end() && content->is_string()) {
    const auto value = content->get<std::string>();
    if (value.starts_with(protocol.cancelMessage)) {
      card.kind = GadgetResultKind::Canceled;
      return card;
    }
    if (value.starts_with(protocol.rejectMessage) ||
        value == protocol.interruptForGadgetUse) {
      card.kind = GadgetResultKind::Rejected;
      return card;
    }
  }
  const auto error = param.find("is_error");
  if (error != param.end() && value::truthy(*error))
    card.kind = GadgetResultKind::Error;
  return card;
}

const char* gadgetResultKindName(GadgetResultKind kind) {
  switch (kind) {
    case GadgetResultKind::Hidden: return "hidden";
    case GadgetResultKind::Canceled: return "canceled";
    case GadgetResultKind::Rejected: return "rejected";
    case GadgetResultKind::Error: return "error";
    case GadgetResultKind::Success: return "success";
  }
  throw std::logic_error("Unknown Gadget result kind");
}

} // namespace batbox::interactive::cards_gadget_result
