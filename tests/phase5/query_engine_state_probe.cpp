#include "query_engine/Conversation.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include <iostream>
using namespace batbox;
namespace {
struct Abort final:query_engine::AbortController {
  Json& trace;std::string label;
  Abort(Json& t,std::string l):trace(t),label(std::move(l)){}
  void abort()override{trace.push_back("abort:"+label);}
};
}
int main(){
 Json trace=Json::array();std::string id="first";
 auto stash=std::make_shared<query_engine::FileStateStash>(100);
 query_engine::ConversationImports imports{[&]{trace.push_back("create");return std::make_shared<Abort>(trace,"new");},[&]{return id;}};
 auto config=std::make_shared<query_engine::Configuration>();
 config->initialMessages=std::make_shared<query_engine::Messages>();
 config->initialMessages->push_back(std::make_shared<Json>(Json{{"type","user"},{"uuid","one"}}));
 config->readFileStash=stash;config->abortController=std::make_shared<Abort>(trace,"provided");
 query_engine::Conversation engine(config,imports);
 trace.push_back({"messagesIdentity",engine.getMessages()==config->initialMessages});
 trace.push_back({"cacheIdentity",engine.getReadFileState()==stash});
 config->initialMessages->push_back(std::make_shared<Json>(Json{{"type","assistant"},{"uuid","two"}}));
 Json messages=Json::array();for(const auto& message:*engine.getMessages())messages.push_back(*message);
 trace.push_back({"messages",messages});trace.push_back({"id",engine.getFlightId()});id="second";trace.push_back({"id",engine.getFlightId()});
 engine.setModel("new-model");trace.push_back({"model",*config->userSpecifiedModel});engine.interrupt();
 auto freshConfig=std::make_shared<query_engine::Configuration>();freshConfig->readFileStash=stash;
 query_engine::Conversation fresh(freshConfig,imports);fresh.interrupt();
 auto otherConfig=std::make_shared<query_engine::Configuration>();otherConfig->readFileStash=stash;
 query_engine::Conversation other(otherConfig,imports);
 trace.push_back({"separateArrays",fresh.getMessages()!=other.getMessages()});
 std::cout<<trace.dump()<<'\n';
}
