#pragma once
#include "runtime/AsyncLocalStorage.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace batbox::bat_context {
struct BatContext {
  std::string batId;
  std::string batType;
  std::optional<std::string> parentFlightId;
  std::optional<std::string> batName,teamName,batColor;
  std::optional<bool> planModeRequired,isTeamLead;
  std::optional<std::string> subagentName;
  std::optional<bool> isBuiltIn;
};
using BatRef=std::shared_ptr<BatContext>;
runtime::AsyncLocalStorage<BatContext>& batContextStorage();
BatRef getBatContext();
template<class Fn>decltype(auto) runWithBatContext(BatRef context,Fn&& fn){
  return batContextStorage().run(std::move(context),std::forward<Fn>(fn));
}
bool isSubagentContext(const BatRef& context);
bool isPartnerBatContext(const BatRef& context,
  const std::function<bool()>& isBatSwarmsEnabled);
}
