// Queued leader poetry: which commands get a row, how rows move from queued
// to delivering to gone, and how the newest message comes back for editing.
#include "interactive/submit/QueuedInputs.hpp"
#include "interactive/TranscriptRows.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::submit;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
CommandRef command(Json value){return std::make_shared<Json>(std::move(value));}
CommandRef poetry(const std::string& id,const std::string& text){
  return command({{"value",text},{"preExpansionValue",text},{"mode","prompt"},
    {"uuid",id},{"priority","next"}});
}
MessageRef userMessage(const std::string& id,const std::string& text){
  return std::make_shared<Json>(Json{{"type","user"},{"uuid",id},
    {"message",{{"role","user"},{"content",text}}}});
}
MessageRef delivered(const std::string& source,const std::string& text){
  return std::make_shared<Json>(Json{{"type","attachment"},{"uuid","attachment-"+source},
    {"attachment",{{"type","queued_command"},{"prompt",text},{"source_uuid",source},
      {"commandMode","prompt"}}}});
}
std::string describe(const std::vector<QueuedInput>& rows){
  std::string text;
  for(const auto& row:rows)text+=row.id+(row.delivering?"+":"")+" ";
  return text;
}
void expect(const QueuedInputTracker& tracker,const std::string& wanted,const std::string& step){
  const auto actual=describe(tracker.rows());
  check(actual==wanted,step+": expected rows \""+wanted+"\", got \""+actual+"\"");
}

void describesOnlyLeaderPoetry(){
  const auto prompt=describeQueuedCommand(command({{"value","expanded paste"},
    {"preExpansionValue","[Pasted text #1 +20 lines]"},{"mode","prompt"},{"uuid","p"}}));
  check(prompt&&prompt->kind==QueuedInput::Kind::Poetry&&
    prompt->text=="[Pasted text #1 +20 lines]"&&prompt->id=="p",
    "poetry rows show what was typed, before paste expansion");
  const auto slash=describeQueuedCommand(command({{"value","/model"},{"mode","prompt"}}));
  check(slash&&slash->kind==QueuedInput::Kind::Command,"slash commands get a command row");
  check(slash->id.starts_with("queued:"),"commands without a uuid are keyed by address");
  const auto shell=describeQueuedCommand(command({{"value","make test"},{"mode","bash"},{"uuid","s"}}));
  check(shell&&shell->kind==QueuedInput::Kind::Shell&&shell->text=="make test",
    "! runs get a shell row");
  check(!describeQueuedCommand(command({{"value","done"},{"mode","task-notification"}})),
    "background notifications are not the user's poetry");
  check(!describeQueuedCommand(command({{"value","hi"},{"mode","prompt"},{"isMeta",true}})),
    "meta commands stay hidden");
  check(!describeQueuedCommand(command({{"value","hi"},{"mode","prompt"},{"agentId","bat-1"}})),
    "Bat-scoped commands belong to the Bat view");
  check(!describeQueuedCommand(command({{"value","   "},{"mode","prompt"}})),
    "blank commands draw nothing");
}

void midTurnDeliveryHasNoDuplicateFrame(){
  QueuedInputTracker tracker;
  std::vector<MessageRef> messages{userMessage("first","start")};
  const auto a=poetry("a","also run the lexer tests"),b=poetry("b","and the parser");
  check(tracker.syncQueue({a},messages),"enqueue draws a row");
  expect(tracker,"a ","one queued");
  check(tracker.syncQueue({a,b},messages),"second enqueue draws a row");
  expect(tracker,"a b ","stacked in order");
  check(!tracker.syncQueue({a,b},messages),"an unchanged queue changes nothing");
  // The Gadget round emits the attachment before the queue drops the command.
  messages.push_back(delivered("a","also run the lexer tests"));
  check(tracker.syncMessages(messages),"delivery removes the queued row at once");
  expect(tracker,"b ","delivered row gone while still queued");
  check(!tracker.syncQueue({a,b},messages),"a delivered command is not drawn again");
  expect(tracker,"b ","no duplicate after a queue notification");
  tracker.syncQueue({b},messages);
  expect(tracker,"b ","consumed command leaves no delivering row");
}

void nextTurnDeliveryBridgesTheGap(){
  QueuedInputTracker tracker;
  std::vector<MessageRef> messages;
  const auto a=poetry("a","after the answer");
  tracker.syncQueue({a},messages);
  // The queue processor takes the command; hooks run before its message exists.
  check(tracker.syncQueue({},messages),"dequeue changes the row");
  expect(tracker,"a+ ","row stays, drawn as sent, until its message exists");
  messages.push_back(userMessage("a","after the answer"));
  check(tracker.syncMessages(messages),"the user message replaces the row");
  expect(tracker,"","handoff complete");
  tracker.syncQueue({poetry("c","blocked by a hook")},messages);
  tracker.syncQueue({},messages);
  expect(tracker,"c+ ","delivering");
  check(tracker.settle(),"turn end settles rows whose message never came");
  expect(tracker,"","settled");
}

void commandsAndWithdrawnRowsLeaveAtOnce(){
  QueuedInputTracker tracker;
  std::vector<MessageRef> messages;
  const auto slash=command({{"value","/compact"},{"mode","prompt"}});
  const auto shell=command({{"value","ls"},{"mode","bash"},{"uuid","s"}});
  tracker.syncQueue({slash,shell},messages);
  check(tracker.rows().size()==2,"commands and ! runs are drawn while queued");
  tracker.syncQueue({},messages);
  expect(tracker,"","commands and ! runs show their own rows once they start");
  const auto a=poetry("a","edit me"),b=poetry("b","keep me");
  tracker.syncQueue({a,b},messages);
  tracker.withdraw({b});
  tracker.syncQueue({a},messages);
  expect(tracker,"a ","withdrawn poetry goes back to the input, not to the model");
}

void deliveredPoetryIsAUserRow(){
  PresentationState state;
  state.messages={delivered("a","also run the lexer tests"),
    std::make_shared<Json>(Json{{"type","attachment"},{"uuid","n"},
      {"attachment",{{"type","queued_command"},{"prompt","done"},
        {"commandMode","task-notification"}}}})};
  rebuildTranscriptRows(state);
  check(state.transcriptRows.size()==2&&
    state.transcriptRows[0].kind==TranscriptRow::Kind::User&&
    state.transcriptRows[0].text=="also run the lexer tests",
    "inline shows delivered poetry as the user's row, not [queued_command]");
  check(state.transcriptRows[1].text=="[queued_command]",
    "other queued attachments keep their placeholder");
  // The note that poetry arrived while the model worked is for the model only.
  state.messages={std::make_shared<Json>(Json{{"type","user"},{"uuid","m"},{"isMeta",true},
    {"message",{{"role","user"},{"content","<system-reminder>note</system-reminder>"}}}}),
    userMessage("u","after the answer")};
  rebuildTranscriptRows(state);
  check(state.transcriptRows.size()==1&&state.transcriptRows[0].text=="after the answer",
    "inline hides meta user messages as full-screen does");
}

void popsTheNewestShownMessage(){
  event_loop::Loop loop;
  CommandQueue queue(loop);
  queue.enqueue(Json{{"value","first"},{"mode","prompt"},{"uuid","1"}});
  queue.enqueue(Json{{"value","bat follow-up"},{"mode","prompt"},{"agentId","bat"}});
  queue.enqueue(Json{{"value","second"},{"mode","prompt"},{"uuid","2"},
    {"pastedContents",{{"1",{{"id",1},{"type","image"},{"content","aGk="},
      {"mediaType","image/png"}}}}}});
  queue.enqueue(Json{{"value","notice"},{"mode","task-notification"}});
  const auto shown=[](const CommandRef& item){return describeQueuedCommand(item).has_value();};
  const auto newest=queue.popEditable(shown,true,"",0,0);
  check(newest&&newest->text=="second","Up takes the newest leader message");
  check(newest->images.size()==1,"its pasted image comes back too");
  check(queue.size()==3,"only that message leaves the queue");
  const auto rest=queue.popEditable(shown,false,"draft",2,0);
  check(rest&&rest->text=="first\ndraft","Esc returns every leader message ahead of the draft");
  check(queue.size()==2,"Bat follow-ups and notifications stay queued");
  check(!queue.popEditable(shown,true,"",0,0),"nothing left to pull back");
}
}

int main(){
  try{
    describesOnlyLeaderPoetry();
    midTurnDeliveryHasNoDuplicateFrame();
    nextTurnDeliveryBridgesTheGap();
    commandsAndWithdrawnRowsLeaveAtOnce();
    deliveredPoetryIsAUserRow();
    popsTheNewestShownMessage();
    std::cout<<"queued poetry rows follow the queue without gaps or duplicates\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
