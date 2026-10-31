#include "interactive/navigation/Cancel.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::navigation;
Json run(const std::string& kind){
  event_loop::Loop loop;
  submit::CommandQueue queue(loop);
  std::vector<std::string> trace;
  if(kind!="empty"){
    Json queued={{"mode","prompt"},{"value","queued"}};
    if(kind=="image")queued["pastedContents"]={{"7",{
      {"id",7},{"type","image"},{"content","new"},
      {"mediaType","image/png"}}}};
    queue.enqueue(queued);
  }
  Json pasted={{"old",{{"id",3},{"type","image"},{"content","old"}}}};
  QueueCancelServices services(queue);
  services.nowMs=[]{return 100.0;};
  services.setInputValue=[&](const std::string& text){
    trace.push_back("input:"+text);
  };
  services.setInputMode=[&](const std::string& mode){
    trace.push_back("mode:"+mode);
  };
  services.updatePastedContents=[&](const std::function<Json(const Json&)>& update){
    pasted=update(pasted);trace.push_back("pasted");
  };
  // The queue's tested pop implementation supplies the result.
  trace.push_back("pop:draft:0");
  handleQueuedCommandOnCancel("draft",services);
  return Json{{"trace",trace},{"pasted",pasted}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  for(const auto& kind:{"empty","text","image"})
    std::cout<<run(kind).dump()<<'\n';
}
