#include "interactive/MessageListPipeline.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
using batbox::Json;
namespace ix=batbox::interactive;
namespace {
ix::MessageRef message(Json data){return std::make_shared<Json>(std::move(data));}
ix::MessageListSettings settings(){
  ix::MessageListSettings s;
  s.gadgets=std::make_shared<ix::GroupingGadgetSet>();
  s.backgroundProtocol={"task-notification","status","summary","Background command "};
  s.visibility={"NO_CONTENT","INTERRUPT",[](const std::string&){return false;}};
  s.readSearch.classification.feature=[](const std::string&){return false;};
  s.readSearch.classification.isFullscreen=[]{return false;};
  return s;
}
} // namespace
int main(){
  ix::MessageRefs source{
    message(Json{{"type","user"},{"uuid","u"},
      {"message",Json{{"content",Json::array({Json{{"type","text"},{"text","hello"}}})}}}}),
    message(Json{{"type","progress"},{"uuid","p"},
      {"data",Json{{"type","task_progress"}}}}),
    message(Json{{"type","attachment"},{"uuid","hidden"},
      {"attachment",Json{{"type","hook_success"}}}}),
    message(Json{{"type","assistant"},{"uuid","a"},
      {"message",Json{{"id","api"},{"content",Json::array({Json{{"type","text"},{"text","reply"}}})}}}})
  };
  std::optional<ix::SliceAnchor> anchor;
  batbox::messages::Services services;
  std::size_t uuidCounter=0;
  services.randomUUID=[&]{std::ostringstream o;o<<"00000000-0000-4000-8000-"<<std::hex<<std::setfill('0')<<std::setw(12)<<++uuidCounter;return o.str();};
  services.nowISO=[]{return "2026-03-31T12:34:56.000Z";};
  const auto normalizedRefs=ix::normalizeMessagesRefs(source,services);
  assert(normalizedRefs[0]!=source[0]&&normalizedRefs[1]==source[1]&&
         normalizedRefs[2]==source[2]&&normalizedRefs[3]!=source[3]);
  auto plain=settings();plain.messageServices=&services;
  auto result=ix::prepareMessageList(source,plain,anchor);
  assert(result.normalizedMessages.size()==4);
  assert(result.messagesToShow.size()==2);
  assert(result.messagesToShow[0]!=source[0]&&result.messagesToShow[1]!=source[3]);
  assert(result.renderable->size()==2);
  assert(result.hiddenMessageCount==-28);
  assert(!result.hasTruncatedMessages);
  auto transcript=settings();transcript.messageServices=&services;transcript.transcriptMode=true;
  transcript.fullscreen=true;transcript.showAllInTranscript=false;
  auto next=ix::prepareMessageList(source,transcript,anchor);
  assert(next.renderable->size()==2);
  auto withBoundary=source;
  withBoundary.push_back(message(Json{{"type","system"},{"subtype","compact_boundary"},
    {"uuid","boundary"},{"content","compacted"}}));
  withBoundary.push_back(message(Json{{"type","assistant"},{"uuid","post"},
    {"message",Json{{"id","post"},{"content",Json::array({Json{{"type","text"},{"text","after"}}})}}}}));
  auto compacted=ix::prepareMessageList(withBoundary,plain,anchor);
  assert(compacted.messagesToShow.size()==2);
  assert(compacted.messagesToShow[0]==withBoundary[4]);
  auto full=ix::prepareMessageList(withBoundary,transcript,anchor);
  assert(full.messagesToShow.size()==4);
  std::cout<<"5 native message pipeline and identity cases pass\n";
}
