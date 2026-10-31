#include "interactive/submit/CommandQueue.hpp"
#include <iostream>

using namespace batbox;
namespace sub=batbox::interactive::submit;
using Out=nlohmann::ordered_json;

static Out values(const sub::CommandList& commands){
  Out result=Out::array();
  for(const auto& command:commands)result.push_back((*command)["value"]);
  return result;
}
static void print(const std::string& name,const Out& result,
                  sub::CommandQueue& queue,const Out& log){
  std::cout<<Out{{"name",name},{"result",result},
    {"remaining",values(queue.get())},{"log",log}}.dump()<<'\n';
}
int main(){
  event_loop::Loop owner;
  Out log=Out::array();
  sub::CommandQueue queue(owner,{[&](const std::string& operation,
      const std::optional<std::string>& content){
    log.push_back(Out::array({operation,content?Out(*content):Out(nullptr)}));
  }});
  queue.enqueue(Out{{"value","agent"},{"mode","prompt"},
    {"priority","now"},{"agentId","sub"}});
  queue.enqueue(Out{{"value","/config"},{"mode","prompt"},
    {"priority","next"}});
  queue.enqueue(Out{{"value","first"},{"mode","prompt"},
    {"priority","now"}});
  queue.enqueue(Out{{"value","!echo hi"},{"mode","bash"},
    {"priority","later"}});
  queue.enqueue(Out{{"value","second"},{"mode","prompt"},
    {"priority","next"}});
  queue.enqueuePendingNotification(Out{{"value","notice"},
    {"mode","task-notification"}});
  Out batches=Out::array();
  for(int i=0;i<4;++i){
    const auto processed=sub::processQueueIfReady(queue,[&](sub::CommandList commands){
      batches.push_back(values(commands));});
    batches.push_back(processed.processed);
  }
  print("priority-and-batching",batches,queue,log);

  queue.reset();log=Out::array();
  queue.enqueue(Out{{"value","/remote"},{"mode","prompt"},
    {"skipSlashCommands",true}});
  Out blocks=Out::array({
    Out{{"type","image"},{"source",Out{{"type","base64"},
      {"data","A"},{"media_type","image/png"}}}},
    Out{{"type","text"},{"text","/array"}}});
  queue.enqueue(Out{{"value",blocks},{"mode","prompt"}});
  queue.enqueue(Out{{"value","ordinary"},{"mode","prompt"}});
  batches=Out::array();
  while(sub::processQueueIfReady(queue,[&](sub::CommandList commands){
    batches.push_back(values(commands));}).processed){}
  print("slash-skip-and-array",batches,queue,log);

  queue.reset();log=Out::array();
  queue.enqueue(Out{{"value","one"},{"mode","prompt"},
    {"priority","later"}});
  queue.enqueue(Out{{"value","system"},{"mode","task-notification"},
    {"priority","now"}});
  Out imageBlock=Out{{"type","image"},{"source",Out{{"type","base64"},
    {"data","abc"},{"media_type","image/png"}}}};
  queue.enqueue(Out{{"value",Out::array({Out{{"type","text"},
    {"text","two"}},imageBlock})},{"mode","prompt"},
    {"pastedContents",Out{{"3",Out{{"id",3},{"type","image"},
      {"content","xyz"}}}}}});
  const auto refs=queue.get();
  const auto popped=queue.popAllEditable("draft",2,1000);
  const auto before=values(queue.getByMaxPriority("now"));
  queue.remove({refs[1]});
  Out result={{"popped",Out{{"text",popped->text},
    {"cursorOffset",popped->cursorOffset},{"images",popped->images}}},
    {"before",before},{"removedIdentity",queue.size()}};
  print("identity-and-edit",result,queue,log);
}
