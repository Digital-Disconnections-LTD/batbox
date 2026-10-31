#pragma once
#include "instructions/hooks/runtime/PoetryHookExecutor.hpp"
#include "query/SimpleQueriesBinding.hpp"

namespace batbox::instructions::hooks::runtime {
using PoetryModelBackend=std::function<event_loop::Deferred<model::MessageRef>(
  model::SimpleQueryRequest)>;
void bindPoetryModelBackend(event_loop::Loop& loop,PoetryHookServices& poetry,
  PoetryModelBackend backend);
void bindPoetryOwnerModel(event_loop::Loop& loop,PoetryHookServices& poetry,
  query::SimpleQueryFactory factory,
  query::OwnerPreflightAwaits awaitedImports={},
  query::OwnerRequestAwaits requestAwaits={});
} // namespace batbox::instructions::hooks::runtime
