#include "interactive/submit/Workload.hpp"
#include <memory>

namespace batbox::interactive::submit {
namespace {
runtime::AsyncContextKeyRef workloadKey(){
  static auto key=std::make_shared<runtime::AsyncContextKey>();
  return key;
}
}
WorkloadScope::WorkloadScope(std::optional<std::string> workload):
    scope_(runtime::extendAsyncContext(workloadKey(),
      std::make_shared<std::optional<std::string>>(
        std::move(workload)))){}
} // namespace batbox::interactive::submit
