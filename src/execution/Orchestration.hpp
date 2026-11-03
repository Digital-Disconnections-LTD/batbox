#pragma once
#include "execution/GadgetExecution.hpp"

namespace batbox::execution {
struct Batch {
  bool isConcurrencySafe=false;
  std::vector<Json> blocks;
};
// newContext refers to the context runGadgets owns for the duration of the
// yield; consumers copy what they keep.
struct MessageUpdate {
  std::optional<Json> message;
  const Context& newContext;
};

int getMaxGadgetUseConcurrency(const std::string& configuredValue);
std::vector<Batch> partitionGadgetCalls(const std::vector<Json>& gadgetUseMessages,const Context& context);
void runGadgets(const std::vector<Json>& gadgetUseMessages,const std::vector<Json>& assistantMessages,
  Context context,Services& services,const std::string& concurrencyValue,
  const std::function<void(const MessageUpdate&)>& yield);
} // namespace batbox::execution
