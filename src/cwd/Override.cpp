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
std::string pwd(const State& state){return currentOverride().value_or(state.cwd);}
std::string getCwd(const State& state){return pwd(state);}
std::string pwd(const std::function<std::string()>& getter){
  const auto override=currentOverride();return override?*override:getter();
}
std::string getCwd(const std::function<std::string()>& getter,
  const std::function<std::string()>& original){
  try{return pwd(getter);}catch(...){return original();}
}
} // namespace batbox::cwd
