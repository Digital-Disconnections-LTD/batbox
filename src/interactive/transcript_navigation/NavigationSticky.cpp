#include "interactive/transcript_navigation/Navigation.hpp"
#include "interactive/message_actions/MessageActions.hpp"
#include <algorithm>

namespace batbox::interactive::transcript_navigation {
namespace {
bool space(char16_t c){
  return (c>=9&&c<=13)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
std::string trimStart(const std::string& text){
  const auto units=value::utf16CodeUnits(text);
  std::size_t start=0;
  while(start<units.size()&&space(units[start]))++start;
  return value::fromUtf16(std::u16string_view(units).substr(start));
}
std::string textField(const Json& value,const char* name){
  const auto& item=value::property(value,name);
  return item.is_string()?item.get<std::string>():std::string{};
}
std::string queuedPoetry(const Json& poetry){
  if(poetry.is_string())return poetry.get<std::string>();
  if(!poetry.is_array())return {};
  std::string result;bool first=true;
  for(const auto& block:poetry){
    if(value::property(block,"type")!="text")continue;
    if(!first)result+='\n';
    first=false;
    result+=textField(block,"text");
  }
  return result;
}
}
std::optional<std::string> stickyPoetryText(const RenderItem& item){
  if(!item.message)return {};
  const auto& message=*item.message;
  const auto& type=value::property(message,"type");
  std::optional<std::string> raw;
  if(type=="user"){
    if(value::truthy(value::property(message,"isMeta"))||
        value::truthy(value::property(message,"isVisibleInTranscriptOnly")))
      return {};
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!content.is_array()||content.empty()||
        value::property(content[0],"type")!="text")return {};
    raw=textField(content[0],"text");
  }else if(type=="attachment"){
    const auto& attachment=value::property(message,"attachment");
    if(value::property(attachment,"type")!="queued_command"||
        value::property(attachment,"commandMode")=="task-notification"||
        value::truthy(value::property(attachment,"isMeta")))return {};
    raw=queuedPoetry(value::property(attachment,"prompt"));
  }
  if(!raw)return {};
  auto text=message_actions::stripSystemReminders(*raw);
  if(text.empty()||text.front()=='<')return {};
  return text;
}
std::string collapseStickyPoetry(const std::string& text){
  auto units=value::utf16CodeUnits(trimStart(text));
  std::size_t paragraph=units.size();
  for(std::size_t i=0;i<units.size();++i){
    if(units[i]!=u'\n')continue;
    std::size_t next=i+1;
    while(next<units.size()&&space(units[next])){
      if(units[next]==u'\n'){paragraph=i;break;}
      ++next;
    }
    if(paragraph!=units.size())break;
  }
  units.resize(std::min<std::size_t>(paragraph,units.size()));
  units.resize(std::min<std::size_t>(500,units.size()));
  std::u16string collapsed;
  bool pendingSpace=false;
  for(const auto c:units){
    if(space(c)){pendingSpace=true;continue;}
    if(pendingSpace&&!collapsed.empty())collapsed.push_back(u' ');
    pendingSpace=false;
    collapsed.push_back(c);
  }
  return value::fromUtf16(collapsed);
}
std::optional<std::string> Navigation::stashedStickyText(
    const RenderItem& item){
  if(!item.message)return {};
  const auto found=stickyTextStash_.find(item.message);
  if(found!=stickyTextStash_.end())return found->second;
  auto result=stickyPoetryText(item);
  stickyTextStash_[item.message]=result;
  if(++stickyStashLookups_%32==0){
    for(auto cursor=stickyTextStash_.begin();cursor!=stickyTextStash_.end();)
      if(cursor->first.expired())cursor=stickyTextStash_.erase(cursor);
      else ++cursor;
  }
  return result;
}
Navigation::StickyCandidate Navigation::stickyCandidate(){
  StickyCandidate candidate;
  if(!items_||items_->empty())return candidate;
  const auto& view=services_.viewport;
  const auto range=view.visible().mounted;
  const auto end=std::min(range.end,items_->size());
  const auto start=std::min(range.start,end);
  const auto target=std::max(0.0,view.scroll().top+
    view.scroll().pendingDelta);
  std::size_t firstVisible=start;
  double firstVisibleTop=-1;
  for(auto index=static_cast<std::ptrdiff_t>(end)-1;
      index>=static_cast<std::ptrdiff_t>(start);--index){
    const auto top=view.ranges().itemTop(static_cast<std::size_t>(index));
    if(top>=0){
      if(top<target)break;
      firstVisibleTop=top;
    }
    firstVisible=static_cast<std::size_t>(index);
  }
  if(firstVisible>0&&!view.scroll().sticky){
    for(auto index=static_cast<std::ptrdiff_t>(firstVisible)-1;
        index>=0;--index){
      const auto text=stashedStickyText(
        items_->at(static_cast<std::size_t>(index)));
      if(!text)continue;
      const auto top=view.ranges().itemTop(
        static_cast<std::size_t>(index));
      if(top>=0&&top+1>=target)continue;
      candidate.index=index;
      candidate.text=*text;
      break;
    }
  }
  const auto origin=firstVisibleTop>=0?
    firstVisibleTop-view.ranges().offset(firstVisible):0.0;
  if(candidate.index>=0)
    candidate.estimate=std::max(0.0,origin+view.ranges().offset(
      static_cast<std::size_t>(candidate.index)));
  return candidate;
}
void Navigation::publishSticky(StickyPoetry value){
  sticky_=std::move(value);
  services_.setStickyPoetry(sticky_);
}
void Navigation::observeSticky(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Sticky poetry owner required");
  candidate_=stickyCandidate();
  // Publish before applying the position correction.
  if(pendingStickyIndex_<0){
    if(suppress_==Suppress::Armed)suppress_=Suppress::Force;
    else{
      const bool force=suppress_==Suppress::Force;
      suppress_=Suppress::None;
      if(force||lastStickyIndex_!=candidate_.index){
        lastStickyIndex_=candidate_.index;
        if(candidate_.index<0)publishSticky({});
        else{
          auto collapsed=collapseStickyPoetry(candidate_.text);
          if(collapsed.empty())publishSticky({});
          else{
            stickyEstimate_=candidate_.estimate;
            publishSticky(StickyPoetry{StickyPoetry::Kind::Poetry,
              std::move(collapsed),
              static_cast<std::size_t>(candidate_.index)});
          }
        }
      }
    }
  }
  if(pendingStickyIndex_>=0){
    if(services_.scrollToMounted(
        static_cast<std::size_t>(pendingStickyIndex_),1)){
      pendingStickyIndex_=-1;pendingStickyTries_=0;
    }else if(++pendingStickyTries_>5){
      pendingStickyIndex_=-1;pendingStickyTries_=0;
    }
  }
}
void Navigation::clickStickyPoetry(){
  if(sticky_.kind!=StickyPoetry::Kind::Poetry)return;
  const auto index=sticky_.index;
  const auto estimate=stickyEstimate_;
  publishSticky(StickyPoetry{StickyPoetry::Kind::Clicked,{},{}});
  suppress_=Suppress::Armed;
  if(services_.scrollToMounted(index,1))return;
  services_.viewport.scrollTo(estimate);
  pendingStickyIndex_=static_cast<std::ptrdiff_t>(index);
  pendingStickyTries_=0;
}
} // namespace batbox::interactive::transcript_navigation
