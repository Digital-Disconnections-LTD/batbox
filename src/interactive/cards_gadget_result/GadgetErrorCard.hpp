#pragma once

#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::interactive::cards_gadget_result {

enum class GadgetErrorKind {
  Interrupted, RejectedPlan, RejectedWithReason,
  ClassifierDenial, GadgetRenderer
};

struct GadgetErrorProtocol {
  std::string interruptForGadgetUse;
  std::string planRejectionPrefix;
  std::string rejectWithReasonPrefix;
  std::string classifierDenialPrefix;
};

struct GadgetErrorCard {
  GadgetErrorKind kind = GadgetErrorKind::GadgetRenderer;
  std::string planContent;
};

GadgetErrorCard classifyGadgetError(const Json& content,
                                bool classifierFeature,
                                const GadgetErrorProtocol& protocol);
const char* gadgetErrorKindName(GadgetErrorKind kind);

} // namespace batbox::interactive::cards_gadget_result
