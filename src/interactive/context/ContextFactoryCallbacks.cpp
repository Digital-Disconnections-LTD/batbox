#include "interactive/context/ContextFactoryInternal.hpp"
#include <stdexcept>

namespace batbox::interactive::context {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Context binding required: ")+name);
}
void updateNested(const std::shared_ptr<app_state::AppStateStore>& store,
                  const char* field,const ValueUpdater& update){
  need(update,"nested state updater");
  store->setState([field,update](const app_state::AppStateRef& previous){
    const auto prior=previous->fieldRef(field);
    const auto next=update(prior);
    if(next==prior)return previous;
    auto changed=std::make_shared<app_state::AppState>(previous->shallowCopy());
    changed->fields->setRef(field,next);
    return changed;
  });
}
}
void bindContextCallbacks(GadgetUseContext& context,
                          std::shared_ptr<const ContextBindings> b){
  context.updateFileHistoryState=[store=b->store](const ValueUpdater& update){
    updateNested(store,"fileHistory",update);
  };
  context.updateAttributionState=[store=b->store](const ValueUpdater& update){
    updateNested(store,"attribution",update);
  };
  context.appendSystemMessage=[set=b->setMessages](const TurnMessageRef& item){
    need(set,"setMessages");
    set([item](const TurnMessagesRef& previous){
      need(previous,"messages");
      auto next=std::make_shared<TurnMessages>(*previous);
      next->push_back(item);
      return next;
    });
  };
  const auto spinner=b->spinner;
  context.onCompactProgress=[spinner](const Json& event){
    const auto& type=value::property(event,"type");
    if(type=="hooks_start"){
      need(spinner.setColor,"spinner color");
      need(spinner.setShimmerColor,"spinner shimmer color");
      need(spinner.setMessage,"spinner message");
      spinner.setColor(spinner.primaryColor);
      spinner.setShimmerColor(spinner.shimmerColor);
      const auto& hook=value::property(event,"hookType");
      spinner.setMessage(hook=="pre_compact"?"Running PreCompact hooks…":
        hook=="post_compact"?"Running PostCompact hooks…":
        "Running SessionStart hooks…");
    }else if(type=="compact_start"){
      need(spinner.setMessage,"spinner message");
      spinner.setMessage("Compacting conversation");
    }else if(type=="compact_end"){
      need(spinner.setMessage,"spinner message");
      need(spinner.setColor,"spinner color");
      need(spinner.setShimmerColor,"spinner shimmer color");
      spinner.setMessage(std::nullopt);
      spinner.setColor(std::nullopt);
      spinner.setShimmerColor(std::nullopt);
    }
  };
}
} // namespace batbox::interactive::context
