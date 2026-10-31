#include "instructions/hooks/runtime/BatOwnerQueryBinding.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
struct StreamState {
  event_loop::Loop& loop;
  std::vector<std::string> consumedCommandUuids;
  std::unique_ptr<query::AsyncEngineIterator> iterator;
  std::shared_ptr<void> sourceContext;
  std::shared_ptr<gadgets::Gadget> structuredGadget;
  explicit StreamState(event_loop::Loop& owner):loop(owner){}
};
}
void bindBatOwnerQuery(event_loop::Loop& loop,BatHookServices& bat,
  BatOwnerQueryBindings bindings){
  if(!bindings.buildContext||!bindings.buildServices)
    throw std::invalid_argument("Agent native query context and services required");
  bat.query=[&loop,bindings=std::move(bindings)](
    BatQueryRequest call){
    if(!loop.isOwnerThread())
      throw std::logic_error("Agent query owner required");
    auto state=std::make_shared<StreamState>(loop);
    state->sourceContext=call.sourceContext;
    state->structuredGadget=call.indigenousStructuredOutputGadget;
    query::Params params;
    params.messages=call.messages;
    params.systemPoetry=call.systemPoetry;
    params.userContext=call.userContext;
    params.systemContext=call.systemContext;
    params.gadgetUseContext=bindings.buildContext(call);
    if(bindings.indigenousContext)
      params.indigenousBatContext=bindings.indigenousContext(call);
    params.querySource=call.querySource;
    auto services=bindings.buildServices(call);
    state->iterator=std::make_unique<query::AsyncEngineIterator>(
      query::makeAsyncQueryLoop(loop,std::move(params),
        state->consumedCommandUuids,std::move(services)));
    BatMessageStream stream;
    stream.next=[state]{
      event_loop::Deferred<GeneratorStep<Json>> output(state->loop);
      state->iterator->next().then([output](const query::AsyncEngineStep* step,
        const std::exception_ptr& failure){
        if(failure){output.reject(failure);return;}
        if(step->done){output.resolve({true,{}});return;}
        if(!step->event){output.reject(std::make_exception_ptr(
          std::runtime_error("Agent query yielded no message")));return;}
        output.resolve({false,*step->event});
      });
      return output;
    };
    stream.close=[state]{
      event_loop::Deferred<GeneratorStep<Json>> output(state->loop);
      state->iterator->returnEarly(Json(nullptr)).then(
        [output](const query::AsyncEngineStep* step,
          const std::exception_ptr& failure){
          if(failure){output.reject(failure);return;}
          output.resolve({true,step->terminal});
        });
      return output;
    };
    return stream;
  };
}
} // namespace batbox::instructions::hooks::runtime
