#pragma once
#include "model/SimpleQueries.hpp"
#include "query/OwnerModelCursor.hpp"

namespace batbox::query {
event_loop::Deferred<model::MessageRef> collectLastAssistantMessage(
  event_loop::Loop& loop,AsyncModelCursor cursor,std::stop_token signal={});
using SimpleQueryFactory=std::function<model::QueryServices(const model::SimpleQueryRequest&)>;
std::function<event_loop::Deferred<model::MessageRef>(model::SimpleQueryRequest)>
  bindSimpleQueryModel(event_loop::Loop& loop,SimpleQueryFactory factory,
    OwnerPreflightAwaits awaitedImports={},OwnerRequestAwaits requestAwaits={});
}
