#include "interactive/cards_gadget_result/GadgetErrorCard.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_result {

GadgetErrorCard classifyGadgetError(const Json& content,
                                bool classifierFeature,
                                const GadgetErrorProtocol& protocol) {
  if (protocol.interruptForGadgetUse.empty() ||
      protocol.planRejectionPrefix.empty() ||
      protocol.rejectWithReasonPrefix.empty() ||
      protocol.classifierDenialPrefix.empty())
    throw std::invalid_argument("Gadget error protocol required");
  if (!content.is_string()) return {};
  const auto text = content.get<std::string>();
  if (text.find(protocol.interruptForGadgetUse) != std::string::npos)
    return {GadgetErrorKind::Interrupted,{}};
  if (text.starts_with(protocol.planRejectionPrefix))
    return {GadgetErrorKind::RejectedPlan,
            text.substr(protocol.planRejectionPrefix.size())};
  if (text.starts_with(protocol.rejectWithReasonPrefix))
    return {GadgetErrorKind::RejectedWithReason,{}};
  if (classifierFeature && text.starts_with(protocol.classifierDenialPrefix))
    return {GadgetErrorKind::ClassifierDenial,{}};
  return {};
}

const char* gadgetErrorKindName(GadgetErrorKind kind) {
  switch (kind) {
    case GadgetErrorKind::Interrupted: return "interrupted";
    case GadgetErrorKind::RejectedPlan: return "rejected_plan";
    case GadgetErrorKind::RejectedWithReason: return "rejected_with_reason";
    case GadgetErrorKind::ClassifierDenial: return "classifier_denial";
    case GadgetErrorKind::GadgetRenderer: return "tool_renderer";
  }
  throw std::logic_error("Unknown Gadget error kind");
}

} // namespace batbox::interactive::cards_gadget_result
