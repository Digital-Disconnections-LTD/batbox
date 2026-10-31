#pragma once
#include "cwd/State.hpp"
#include "runtime/AsyncLocalStorage.hpp"
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace batbox::cwd {
// The owner loop and indigenous launch adapters propagate this storage alongside
// bat/partner context. bindCurrentCwd also supports explicit foreign calls.
std::optional<std::string> currentOverride();
class OverrideScope {
public:
  explicit OverrideScope(std::optional<std::string> value);
  ~OverrideScope();
  OverrideScope(const OverrideScope&)=delete;
  OverrideScope& operator=(const OverrideScope&)=delete;
private:
  runtime::AsyncContextScope scope_;
};
template<class F>auto runWithCwdOverride(const std::string& value,F&& fn)
  ->decltype(std::forward<F>(fn)()){
  OverrideScope scope(value);
  return std::forward<F>(fn)();
}
template<class F>auto bindCurrentCwd(F fn){
  const auto captured=currentOverride();
  return [captured,fn=std::move(fn)](auto&&... args)mutable
    ->decltype(auto){
    OverrideScope scope(captured);
    return fn(std::forward<decltype(args)>(args)...);
  };
}
std::string pwd(const State& state);
std::string getCwd(const State& state);
// Imported state getters can throw before initialization; getCwd uses the
// original cwd exactly when pwd fails.
std::string pwd(const std::function<std::string()>& getCwdState);
std::string getCwd(const std::function<std::string()>& getCwdState,
  const std::function<std::string()>& getOriginalCwd);
} // namespace batbox::cwd
