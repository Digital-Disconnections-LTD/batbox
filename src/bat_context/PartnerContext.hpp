#pragma once
#include "runtime/AsyncLocalStorage.hpp"
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace batbox::bat_context {
struct PartnerSpawnConfig {
  std::string batId,batName,teamName;
  std::optional<std::string> color;
  bool planModeRequired=false;
  std::string parentFlightId;
  std::shared_ptr<std::stop_source> abortController;
};
struct PartnerContext:PartnerSpawnConfig {bool isInProcess=true;};
using PartnerRef=std::shared_ptr<PartnerContext>;
runtime::AsyncLocalStorage<PartnerContext>& partnerContextStorage();
PartnerRef getPartnerContext();
template<class Fn>decltype(auto) runWithPartnerContext(
    PartnerRef context,Fn&& fn){
  return partnerContextStorage().run(std::move(context),std::forward<Fn>(fn));
}
bool isInProcessPartner();
PartnerRef createPartnerContext(const PartnerSpawnConfig& config);
}
