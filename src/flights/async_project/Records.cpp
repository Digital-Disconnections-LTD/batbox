#include "flights/async_project/AsyncProject.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <utility>

namespace batbox::flights::async_project {
Deferred<std::optional<std::string>> AsyncProject::recordTranscriptRefs(
  const std::vector<MessageRef>& messages,const Json& team,
  std::optional<std::string> startingParent,const std::vector<MessageRef>& all){
  Deferred<std::optional<std::string>> result(loop_);
  try{
    const auto cleaned=cleanMessagesForLoggingRefs(messages,all,imports_.cleaning);
    const auto flight=imports_.getFlightId();
    imports_.getFlightMessages(flight).then([this,result,cleaned,team,
      startingParent=std::move(startingParent)](
      const std::shared_ptr<runtime::OrderedStringSet>* recorded,
      const std::exception_ptr& error)mutable{
      if(error){result.reject(error);return;}
      try{
        if(!*recorded)throw std::invalid_argument("session message set missing");
        std::vector<MessageRef> fresh;
        auto parent=startingParent;bool seenNew=false;
        for(const auto& reference:cleaned){
          const auto message=runtime::snapshotMessage(reference);
          const auto uuid=message.at("uuid").get<std::string>();
          if((*recorded)->contains(uuid)){
            if(!seenNew&&isChainParticipant(message))parent=uuid;
          }else{fresh.push_back(reference);seenNew=true;}
        }
        auto finish=[result,fresh,parent]{
          try{
            for(auto it=fresh.rbegin();it!=fresh.rend();++it){
              const auto message=runtime::snapshotMessage(*it);
              if(isChainParticipant(message)){
                result.resolve(message.at("uuid").get<std::string>());return;
              }
            }
            result.resolve(parent);
          }catch(...){result.reject(std::current_exception());}
        };
        if(fresh.empty()){finish();return;}
        insertMessageChainRefs(fresh,false,std::nullopt,parent,team).then(
          [result,finish](const Unit*,const std::exception_ptr& insertError){
          if(insertError)result.reject(insertError);else finish();
        });
      }catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Completion AsyncProject::recordSidechainTranscriptRefs(
  const std::vector<MessageRef>& messages,std::optional<std::string> bat,
  std::optional<std::string> startingParent){
  Completion result(loop_);
  try{
    auto cleaned=cleanMessagesForLoggingRefs(messages,messages,imports_.cleaning);
    insertMessageChainRefs(std::move(cleaned),true,std::move(bat),
      std::move(startingParent)).then([result](const Unit*,const std::exception_ptr& error){
      if(error)result.reject(error);else result.resolve({});
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Completion AsyncProject::insertFileHistorySnapshot(std::string messageId,
  Json snapshot,bool update){
  return event_loop::adoptAsyncReturn(queue_->trackWrite<Unit>([this,messageId=std::move(messageId),
    snapshot=std::move(snapshot),update]{
    return appendEntry(Json{{"type","file-history-snapshot"},{"messageId",messageId},
      {"snapshot",snapshot},{"isSnapshotUpdate",update}});
  }));
}
Completion AsyncProject::insertQueueOperation(Json operation){
  return event_loop::adoptAsyncReturn(queue_->trackWrite<Unit>([this,operation=std::move(operation)]{
    return appendEntry(operation);
  }));
}
Completion AsyncProject::insertAttributionSnapshot(Json snapshot){
  return event_loop::adoptAsyncReturn(queue_->trackWrite<Unit>([this,snapshot=std::move(snapshot)]{
    return appendEntry(snapshot);
  }));
}
Completion AsyncProject::insertContentReplacement(Json replacements,
  std::optional<std::string> bat){
  return event_loop::adoptAsyncReturn(queue_->trackWrite<Unit>([this,replacements=std::move(replacements),
    bat=std::move(bat)]{
    Json entry={{"type","content-replacement"},
      {"sessionId",imports_.getFlightId()}};
    if(bat)entry["agentId"]=*bat;
    entry["replacements"]=replacements;
    return appendEntry(std::move(entry));
  }));
}
Completion AsyncProject::recordContextCollapseCommit(Json commit){
  Completion result(loop_);
  try{
    const auto flight=imports_.getFlightId();
    if(flight.empty()){result.resolve({});return result;}
    Json entry={{"type","marble-origami-commit"},{"sessionId",flight}};
    for(const auto& [key,value]:commit.items())entry[key]=value;
    appendEntry(std::move(entry)).then([result](const Unit*,const std::exception_ptr& error){
      if(error)result.reject(error);else result.resolve({});
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Completion AsyncProject::recordContextCollapseSnapshot(Json snapshot){
  Completion result(loop_);
  try{
    const auto flight=imports_.getFlightId();
    if(flight.empty()){result.resolve({});return result;}
    Json entry={{"type","marble-origami-snapshot"},{"sessionId",flight}};
    for(const auto& [key,value]:snapshot.items())entry[key]=value;
    appendEntry(std::move(entry)).then([result](const Unit*,const std::exception_ptr& error){
      if(error)result.reject(error);else result.resolve({});
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::flights::async_project
