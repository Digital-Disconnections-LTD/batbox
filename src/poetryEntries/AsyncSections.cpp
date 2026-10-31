#include "poetryEntries/AsyncSections.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
struct AsyncSectionStash::State {
  event_loop::Loop::Dispatcher owner;
  std::unordered_map<std::string,SectionValue> stash;
  explicit State(event_loop::Loop& loop):owner(loop.dispatcher()){}
  void checkOwner()const{
    if(!owner.isOwnerThread())throw std::logic_error("Prompt section cache requires owner thread");
  }
};
AsyncPoetrySection systemPoetrySection(std::string name,ComputeAsyncSection compute){
  return {std::move(name),std::move(compute),false};
}
AsyncPoetrySection uncachedSystemPoetrySection(std::string name,
  ComputeAsyncSection compute,const std::string&){
  return {std::move(name),std::move(compute),true};
}
AsyncSectionStash::AsyncSectionStash(event_loop::Loop& loop):state_(std::make_shared<State>(loop)){}
event_loop::Deferred<std::vector<SectionValue>> AsyncSectionStash::resolveSystemPoetrySections(
    const std::vector<AsyncPoetrySection>& sections){
  state_->checkOwner();
  using Pending=event_loop::Deferred<SectionValue>;
  std::vector<Pending> mapped;mapped.reserve(sections.size());
  // Start every asynchronous section callback before observing
  // their outcomes. A synchronous compute throw rejects that callback only.
  for(const auto& section:sections){
    Pending result(state_->owner);
    const auto existing=state_->stash.find(section.name);
    if(!section.stashBreak&&existing!=state_->stash.end())result.resolve(existing->second);
    else try{
      if(!section.compute)throw std::invalid_argument("Missing system prompt section compute binding");
      const auto computed=section.compute();
      auto completed=[state=state_,result,name=section.name](const SectionValue& value,
          const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        try{state->stash[name]=value;result.resolve(value);}
        catch(...){result.reject(std::current_exception());}
      };
      if(const auto* async=std::get_if<Pending>(&computed))
        async->then([completed](const SectionValue* value,const std::exception_ptr& error){
          completed(value?*value:SectionValue{},error);
        });
      else if(const auto* text=std::get_if<event_loop::Deferred<std::string>>(&computed))
        text->then([completed](const std::string* value,const std::exception_ptr& error){
          completed(value?SectionValue(*value):SectionValue{},error);
        });
      else{
        Pending awaited(state_->owner);awaited.resolve(std::get<SectionValue>(computed));
        awaited.then([completed](const SectionValue* value,const std::exception_ptr& error){
          completed(value?*value:SectionValue{},error);
        });
      }
    }catch(...){result.reject(std::current_exception());}
    mapped.push_back(result);
  }
  event_loop::Deferred<std::vector<SectionValue>> all(state_->owner);
  struct Aggregate {std::vector<SectionValue> values;std::size_t remaining;};
  auto aggregate=std::make_shared<Aggregate>(Aggregate{
    std::vector<SectionValue>(mapped.size()),mapped.size()});
  if(mapped.empty())all.resolve({});
  for(std::size_t i=0;i<mapped.size();++i)
    mapped[i].then([all,aggregate,i](const SectionValue* value,const std::exception_ptr& error){
      if(error){all.reject(error);return;}
      aggregate->values[i]=*value;
      if(--aggregate->remaining==0)all.resolve(std::move(aggregate->values));
    });
  return event_loop::adoptAsyncReturn(all);
}
void AsyncSectionStash::clearSystemPoetrySections(const std::function<void()>& clearBetaHeaderLatches){
  state_->checkOwner();state_->stash.clear();
  if(!clearBetaHeaderLatches)throw std::invalid_argument("Missing beta header latch binding");
  clearBetaHeaderLatches();
}
std::optional<SectionValue> AsyncSectionStash::stashed(const std::string& name)const{
  state_->checkOwner();const auto found=state_->stash.find(name);
  if(found==state_->stash.end())return std::nullopt;
  return std::optional<SectionValue>(std::in_place,found->second);
}
}
