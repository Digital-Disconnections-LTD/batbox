#include "cwd/Override.hpp"
#include <utility>

namespace batbox::cwd {
namespace {runtime::AsyncLocalStorage<std::string> overrideStorage;}
std::optional<std::string> currentOverride(){
  const auto stored=overrideStorage.getStore();
  return stored?std::optional<std::string>(*stored):std::nullopt;
}
OverrideScope::OverrideScope(std::optional<std::string> value):
  scope_(overrideStorage.scope(value?std::make_shared<std::string>(std::move(*value)):nullptr)){}
OverrideScope::~OverrideScope()=default;
std::string pwd(const std::function<std::string()>& getter){
  const auto override=currentOverride();return override?*override:getter();
}
} // namespace batbox::cwd
