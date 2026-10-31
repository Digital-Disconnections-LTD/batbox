#include "terminal_reconciler/CommitLifecycle.hpp"
#include "runtime/JsonValue.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace batbox::terminal_reconciler {
namespace {
std::string fixedOne(double value){
  if(!std::isfinite(value)||std::abs(value)>=1e21)
    return value::numberToString(value);
  const bool negative=std::signbit(value);value=std::abs(value);
  const long double scaled=static_cast<long double>(value)*10.0L;
  if(scaled>=static_cast<long double>(
      std::numeric_limits<std::uint64_t>::max())){
    char digits[128];
    const auto [end,error]=std::to_chars(digits,digits+sizeof(digits),value,
      std::chars_format::fixed,1);
    if(error!=std::errc{})
      throw std::runtime_error("Commit timing format failed");
    return (negative?"-":"")+std::string(digits,end);
  }
  const auto lower=std::floor(scaled);
  const auto rounded=static_cast<std::uint64_t>(
    lower+(scaled-lower>=0.5L?1:0));
  return (negative?"-":"")+std::to_string(rounded/10)+"."+
    std::to_string(rounded%10);
}
}
CommitLifecycle::CommitLifecycle(ProfileCounters& profile,
    CommitLifecycleImports imports):profile_(profile),
    imports_(std::move(imports)){}
double CommitLifecycle::now()const{
  if(!imports_.performanceNow)
    throw std::invalid_argument("Reconciler performance clock required");
  return imports_.performanceNow();
}
void CommitLifecycle::append(const std::string& value)const{
  if(!imports_.appendFileSync)
    throw std::invalid_argument("Commit log append import required");
  imports_.appendFileSync(*imports_.commitLogPath,value);
}
void CommitLifecycle::prepareForCommit(){
  if(imports_.commitLogPath&&!imports_.commitLogPath->empty())
    prepareAt_=now();
}
void CommitLifecycle::recordCreatedNode(){
  if(imports_.commitLogPath&&!imports_.commitLogPath->empty())
    ++createCount_;
}
void CommitLifecycle::resetAfterCommit(terminal_dom::DOMElement& root){
  profile_.finishCommit();
  const bool logging=imports_.commitLogPath&&
    !imports_.commitLogPath->empty();
  if(logging){
    const auto current=now();
    ++commits_;
    const auto gap=lastCommitAt_>0?current-lastCommitAt_:0;
    if(gap>maxGapMs_)maxGapMs_=gap;
    lastCommitAt_=current;
    const auto reconcileMs=prepareAt_>0?current-prepareAt_:0;
    if(gap>30||reconcileMs>20||createCount_>50){
      append(fixedOne(current)+" gap="+fixedOne(gap)+
        "ms reconcile="+fixedOne(reconcileMs)+"ms creates="+
        value::numberToString(createCount_)+"\n");
    }
    createCount_=0;
    if(current-lastLog_>1000){
      append(fixedOne(current)+" commits="+
        value::numberToString(commits_)+"/s maxGap="+
        fixedOne(maxGapMs_)+"ms\n");
      commits_=0;maxGapMs_=0;lastLog_=current;
    }
  }
  const auto t0=logging?now():0;
  if(root.onComputeLayout)root.onComputeLayout();
  if(logging){
    const auto layoutMs=now()-t0;
    if(layoutMs>20){
      if(!imports_.getYogaCounters)
        throw std::invalid_argument("Yoga counters import required");
      const auto c=imports_.getYogaCounters();
      append(fixedOne(t0)+" SLOW_YOGA "+fixedOne(layoutMs)+
        "ms visited="+value::numberToString(c.visited)+
        " measured="+value::numberToString(c.measured)+
        " hits="+value::numberToString(c.stashHits)+
        " live="+value::numberToString(c.live)+"\n");
    }
  }
  if(!imports_.isTestMode)
    throw std::invalid_argument("Reconciler test-mode import required");
  if(imports_.isTestMode()){
    if(root.childNodes.empty()&&root.hasRenderedContent.value_or(false))return;
    if(!root.childNodes.empty())root.hasRenderedContent=true;
    if(root.onImmediateRender)root.onImmediateRender();
    return;
  }
  const auto tr=logging?now():0;
  if(root.onRender)root.onRender();
  if(logging){
    const auto renderMs=now()-tr;
    if(renderMs>10)
      append(fixedOne(tr)+" SLOW_PAINT "+fixedOne(renderMs)+"ms\n");
  }
}
} // namespace batbox::terminal_reconciler
