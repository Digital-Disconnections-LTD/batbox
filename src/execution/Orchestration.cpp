#include "execution/Orchestration.hpp"
#include "execution/Internal.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace batbox::execution {
namespace {
const Json& assistantFor(const Json& block,const std::vector<Json>& assistantMessages) {
  const Json& id=internal::field(block,"id");
  for(const Json& assistant:assistantMessages) {
    const Json& content=internal::field(internal::field(assistant,"message"),"content");
    if(!content.is_array())continue;
    for(const Json& item:content)if(internal::field(item,"type")=="tool_use" &&
      internal::field(item,"id")==id)return assistant;
  }
  throw std::runtime_error("No assistant message contains tool use " + id.dump());
}
}

int getMaxGadgetUseConcurrency(const std::string& configuredValue) {
  // parseInt(value, 10) || 10: a leading decimal integer is accepted.
  const char* begin=configuredValue.c_str();
  while(*begin==' '||*begin=='\t'||*begin=='\n')++begin;
  const char* cursor=begin;
  const bool negative=*cursor=='-';
  if(*cursor=='+'||negative)++cursor;
  const char* digits=cursor;
  while(*cursor>='0'&&*cursor<='9')++cursor;
  if(cursor==digits)return 10;
  unsigned long long parsed=0;
  for(const char* p=digits;p<cursor;++p) {
    parsed=std::min<unsigned long long>(static_cast<unsigned long long>(INT_MAX)+1,
      parsed*10+static_cast<unsigned>(*p-'0'));
  }
  if(parsed==0)return 10;
  if(negative)return parsed>=static_cast<unsigned>(INT_MAX)?INT_MIN:-static_cast<int>(parsed);
  return parsed>=static_cast<unsigned>(INT_MAX)?INT_MAX:static_cast<int>(parsed);
}

std::vector<Batch> partitionGadgetCalls(const std::vector<Json>& gadgetUseMessages,const Context& context) {
  std::vector<Batch> batches;
  for(const Json& gadgetUse:gadgetUseMessages) {
    const auto* gadget=gadgets::findGadgetByName(context.gadgets,internal::stringField(gadgetUse,"name"));
    bool safe=false;
    if(gadget&&gadget->parseInput)try {
      const Json parsed=gadget->parseInput(internal::field(gadgetUse,"input"));
      if(gadget->isConcurrencySafe)try{safe=gadget->isConcurrencySafe(parsed);}catch(...){safe=false;}
    }catch(...){safe=false;}
    if(safe&&!batches.empty()&&batches.back().isConcurrencySafe)
      batches.back().blocks.push_back(gadgetUse);
    else batches.push_back(Batch{safe,{gadgetUse}});
  }
  return batches;
}

void runGadgets(const std::vector<Json>& gadgetUseMessages,const std::vector<Json>& assistantMessages,
  Context context,Services& services,const std::string& concurrencyValue,
  const std::function<void(const MessageUpdate&)>& yield) {
  internal::require(yield,"yield");
  const auto batches=partitionGadgetCalls(gadgetUseMessages,context);
  for(const auto& batch:batches) {
    if(!batch.isConcurrencySafe) {
      for(const Json& block:batch.blocks) {
        const std::string id=internal::stringField(block,"id");
        if(services.markGadgetUseInProgress)services.markGadgetUseInProgress(id);
        runGadgetUse(block,assistantFor(block,assistantMessages),context,services,[&](const Update& update){
          if(update.contextModifier)context.data=(*update.contextModifier)(context.data);
          if(update.appStateModifier)appendAppStateModifier(context,*update.appStateModifier);
          yield(MessageUpdate{update.message,context});
        });
        if(services.markGadgetUseComplete)services.markGadgetUseComplete(id);
      }
      continue;
    }
    // all() respects the concurrency cap, resumes producers as values arrive,
    // and starts the next producer when one completes.
    struct Event {std::optional<Update> update;std::exception_ptr error;};
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Event> events;
    std::vector<std::thread> workers;
    std::size_t next=0,active=0,done=0;
    std::map<std::string,std::vector<std::function<Json(const Json&)>>> modifiers;
    std::map<std::string,std::vector<std::function<Json(const Json&)>>> appStateModifiers;
    const int cap=getMaxGadgetUseConcurrency(concurrencyValue);
    auto start=[&](std::size_t index){
      const Json block=batch.blocks[index];
      const Json assistant=assistantFor(block,assistantMessages);
      const std::string id=internal::stringField(block,"id");
      ++active;
      workers.emplace_back([&,block,assistant,id]{
        try {
          if(services.markGadgetUseInProgress)services.markGadgetUseInProgress(id);
          runGadgetUse(block,assistant,context,services,[&](const Update& update){
            std::lock_guard lock(mutex);events.push_back(Event{update,{}});changed.notify_one();
          });
          if(services.markGadgetUseComplete)services.markGadgetUseComplete(id);
          std::lock_guard lock(mutex);events.push_back(Event{});changed.notify_one();
        }catch(...) {
          std::lock_guard lock(mutex);events.push_back(Event{{},std::current_exception()});changed.notify_one();
        }
      });
    };
    for(;next<batch.blocks.size()&&static_cast<int>(active)<cap;++next)start(next);
    std::exception_ptr failure;
    while(done<batch.blocks.size() && (active>0||!events.empty())) {
      Event event;
      {
        std::unique_lock lock(mutex);
        changed.wait(lock,[&]{return !events.empty();});
        event=std::move(events.front());events.pop_front();
      }
      if(event.error) {failure=event.error;break;}
      if(event.update) {
        const auto& update=*event.update;
        if(update.contextModifier)modifiers[update.gadgetUseID].push_back(*update.contextModifier);
        if(update.appStateModifier)appStateModifiers[update.gadgetUseID].push_back(
          *update.appStateModifier);
        try{yield(MessageUpdate{update.message,context});}catch(...){failure=std::current_exception();break;}
      } else {
        --active;++done;
        if(next<batch.blocks.size())start(next++);
      }
    }
    for(auto& worker:workers)if(worker.joinable())worker.join();
    if(failure)std::rethrow_exception(failure);
    for(const Json& block:batch.blocks) {
      const auto id=internal::stringField(block,"id");
      for(const auto& modifier:modifiers[id])context.data=modifier(context.data);
      for(const auto& modifier:appStateModifiers[id])appendAppStateModifier(context,modifier);
    }
    yield(MessageUpdate{{},context});
  }
}
} // namespace batbox::execution
