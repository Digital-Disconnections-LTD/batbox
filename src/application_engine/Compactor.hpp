#pragma once
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "compaction/Restore.hpp"
#include "compaction/Summary.hpp"
#include "query/Setup.hpp"
#include "services/tokenEstimation.hpp"
#include <vector>

namespace batbox::application_engine {
struct CompactorInputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<messages::Services> messages;
  compaction::WorkflowServices workflow;
  compaction::RestoreServices restore;
  std::function<compaction::RestoreServices(const execution::Context&)> makeRestore;
  compaction::SummaryServices summary;
  std::function<Json(const execution::Context&)> getAppState;
  std::function<bool()> autoCompactEnabled;
  std::function<Json()> compactionSettings;
  std::function<Json(const Json&,const gadgets::Gadgets&)> normalize;
  AttachmentNormalizer normalizeAttachment;
  std::function<void(const Json&)> progress,notification;
  std::function<void()> markPostCompaction;
  std::function<void(const std::optional<std::string>&)> cleanupAfterCompact;
  std::function<void(const std::vector<std::string>&)> clearInvokedSkills;
  explicit CompactorInputs(event_loop::Loop& loop):owner(loop){}
};
class Compactor final:public std::enable_shared_from_this<Compactor> {
public:
  static std::shared_ptr<Compactor> create(CompactorInputs inputs);
  event_loop::Deferred<Json> compact(const Json& messages,
    execution::Context context,Json stash,bool automatic,
    std::optional<std::string> instructions={},
    std::shared_ptr<bats::StashSafeParams> indigenous={});
  event_loop::Deferred<Json> automatic(const Json& messages,
    const execution::Context& context,const Json& stash,
    const std::string& source,const Json& tracking,double freed,
    std::shared_ptr<bats::StashSafeParams> indigenous={});
  void postCompactCleanup(const std::optional<std::string>& source={});
  void clearInvokedSkills(const std::vector<std::string>& preservedBatIds);
private:
  explicit Compactor(CompactorInputs inputs):inputs_(std::move(inputs)){}
  compaction::Context contextFor(const execution::Context& context,
    std::shared_ptr<bats::StashSafeParams> indigenous);
  compaction::WorkflowServices workflowFor(const execution::Context& context);
  compaction::SummaryServices summaryFor(const execution::Context& context);
  compaction::AutoServices automaticServices(const execution::Context& context,
    std::shared_ptr<bats::StashSafeParams> indigenous);
  CompactorInputs inputs_;
};
} // namespace batbox::application_engine
