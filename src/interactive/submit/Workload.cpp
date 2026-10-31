#include "interactive/submit/Workload.hpp"
#include <memory>

namespace batbox::interactive::submit {
namespace {
runtime::AsyncContextKeyRef workloadKey(){
  static auto key=std::make_shared<runtime::AsyncContextKey>();
  return key;
}
}
std::optional<std::string> getWorkload(){
  auto value=runtime::currentAsyncContextValue(workloadKey());
  if(!value)return {};
  return *std::static_pointer_cast<std::optional<std::string>>(value);
}
WorkloadScope::WorkloadScope(std::optional<std::string> workload):
    scope_(runtime::extendAsyncContext(workloadKey(),
      std::make_shared<std::optional<std::string>>(
        std::move(workload)))){}
} // namespace batbox::interactive::submit
