#pragma once
#include "instructions/project/async/AsyncDiscovery.hpp"

namespace batbox::instructions::project::async {
struct ExternalIncludeConfig {bool approved=false,warningShown=false;};
struct ExternalWarningImports {
  std::function<ExternalIncludeConfig()> getCurrentProjectConfig;
  std::function<Deferred<MemoryFiles>()> getForcedMemoryFiles;
  std::function<bool(const MemoryFiles&)> hasExternalIncludes;
};
Deferred<bool> shouldShowExternalIncludesWarning(event_loop::Loop& loop,
  const ExternalWarningImports& imports);
ExternalWarningImports bindExternalWarningImports(MemoryDiscoveryStash& stash,
  Services services,std::function<ExternalIncludeConfig()> config);
} // namespace batbox::instructions::project::async
