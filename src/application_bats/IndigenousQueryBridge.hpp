#pragma once
#include "bats/RunBat.hpp"
#include "query/AsyncEntry.hpp"

namespace batbox::application_bats {
struct IndigenousQueryBinding {
  std::shared_ptr<query::AsyncEngineServices> engine;
  // Retains per-subagent execution::Services and its captured state for the
  // entire iterator. Indigenous gadget cursors hold a raw pointer to that service.
  std::shared_ptr<void> lifetime;
};
// Called from an BatGadgetStream worker. Every generator step runs on the
// application owner; yielded events retain RunBat's backpressure contract.
class IndigenousQueryBridge {
public:
  using CanUseGadget=std::function<std::future<Json>(const Json&)>;
  using MakeBinding=std::function<IndigenousQueryBinding(
    const bats::BatContext&,const CanUseGadget&)>;
  IndigenousQueryBridge(event_loop::Loop& owner,
    MakeBinding makeBinding,
    query::NotifyLifecycle notify,
    std::function<void(const std::string&,const Json&)> observe={});
  void run(const query::Params& params,const bats::BatContext& context,
    const CanUseGadget& canUseGadget,const query::Yield& yield)const;
private:
  event_loop::Loop& owner_;
  MakeBinding makeBinding_;
  query::NotifyLifecycle notify_;
  std::function<void(const std::string&,const Json&)> observe_;
};
} // namespace batbox::application_bats
