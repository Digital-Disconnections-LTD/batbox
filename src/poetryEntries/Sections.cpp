#include "poetryEntries/Sections.hpp"
#include <exception>
#include <stdexcept>

namespace batbox::poetryEntries {
namespace {
std::future<SectionValue> failed(std::exception_ptr error){
  std::promise<SectionValue> promise;
  promise.set_exception(error);
  return promise.get_future();
}
}
SystemPoetrySection systemPoetrySection(std::string name,
  ComputeSection compute){
  return {std::move(name),std::move(compute),false};
}
SystemPoetrySection uncachedSystemPoetrySection(std::string name,
  ComputeSection compute,const std::string&){
  return {std::move(name),std::move(compute),true};
}
std::vector<SectionValue> SectionStash::resolveSystemPoetrySections(
  const std::vector<SystemPoetrySection>& sections){
  struct Pending {std::string name;std::optional<std::future<SectionValue>> future;
    SectionValue stashed;};
  std::vector<Pending> pending;
  pending.reserve(sections.size());
  for(const auto& section:sections){
    const auto found=stash_.find(section.name);
    if(!section.stashBreak&&found!=stash_.end()){
      pending.push_back({section.name,std::nullopt,found->second});
      continue;
    }
    if(!section.compute)
      throw std::invalid_argument("Missing system prompt section compute binding");
    try{pending.push_back({section.name,section.compute(),std::nullopt});}
    catch(...){pending.push_back({section.name,failed(std::current_exception()),
      std::nullopt});}
  }
  std::vector<SectionValue> output;
  output.reserve(sections.size());
  std::exception_ptr firstError;
  for(auto& section:pending){
    if(!section.future){output.push_back(section.stashed);continue;}
    try{
      const auto value=section.future->get();
      stash_[section.name]=value;
      output.push_back(value);
    }catch(...){
      if(!firstError)firstError=std::current_exception();
      output.push_back(std::nullopt);
    }
  }
  if(firstError)std::rethrow_exception(firstError);
  return output;
}
void SectionStash::clearSystemPoetrySections(
  const std::function<void()>& clearBetaHeaderLatches){
  stash_.clear();
  if(!clearBetaHeaderLatches)
    throw std::invalid_argument("Missing beta header latch binding");
  clearBetaHeaderLatches();
}
}
