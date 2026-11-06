#include "query/IndigenousGadgetCursor.hpp"
#include "query/CloseableIndigenousCursor.hpp"

namespace batbox::query {
namespace {
// AsyncIteratorClose stops the gadget-update generator at its suspended yield.
// This indigenous signal bypasses runGadgetUse's std::exception error-to-gadget-result
// conversion; it is consumed only by the closeable cursor adapter.
struct GadgetCursorClosed {};
bool isGadgetCursorClosed(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const GadgetCursorClosed&){return true;}
  catch(...){return false;}
  return false;
}
}
AsyncGadgetUpdateCursor makeIndigenousGadgetUpdatesCursor(event_loop::Loop& loop,
  const std::vector<Json>& blocks,const Json& assistantMessages,
  const std::shared_ptr<PreparedIteration>& prepared,
  execution::Services& executionServices,const std::string& concurrencyValue){
  if(!prepared)throw std::invalid_argument("Native tool cursor requires prepared iteration");
  IndigenousProducer<AsyncGadgetUpdate> producer=[blocks,assistantMessages,prepared,
      &executionServices,concurrencyValue](
      const std::function<void(const AsyncGadgetUpdate&)>& yield,std::stop_token stop){
    std::vector<Json> assistants;
    if(assistantMessages.is_array())
      for(const Json& message:assistantMessages)assistants.push_back(message);
    execution::runGadgets(blocks,assistants,prepared->gadgetUseContext,
      executionServices,concurrencyValue,
      [&](const execution::MessageUpdate& update){
        if(stop.stop_requested())throw GadgetCursorClosed{};
        AsyncGadgetUpdate item;
        if(update.message)item.message=std::make_shared<Json>(*update.message);
        item.newContext=update.newContext;
        yield(item);
        if(stop.stop_requested())throw GadgetCursorClosed{};
      });
  };
  auto indigenous=makeCloseableIndigenousCursor<AsyncGadgetUpdate>(loop,
    std::move(producer),{},isGadgetCursorClosed);
  return AsyncGadgetUpdateCursor{[indigenous]{return indigenous.next();},
    [indigenous]{return indigenous.close();}};
}
void bindIndigenousGadgetUpdates(event_loop::Loop& loop,AsyncGadgetTurnServices& services){
  auto* executionServices=services.sync.executionServices;
  const auto concurrency=services.sync.concurrencyValue;
  services.gadgetUpdates=[&loop,executionServices,concurrency](
      const std::vector<Json>& blocks,const Json& assistants,
      const std::shared_ptr<PreparedIteration>& prepared){
    if(!executionServices)
      throw std::invalid_argument("Unbound native tool execution services");
    return makeIndigenousGadgetUpdatesCursor(loop,blocks,assistants,prepared,
      *executionServices,concurrency);
  };
}
} // namespace batbox::query
