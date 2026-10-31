#include "bootstrap/RequestState.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::RequestState state;
    std::shared_ptr<Json> request,messages,classifiers;
    bool requestSet=false,messagesSet=false,classifiersSet=false;
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="request"){
        requestSet=true;
        if(op["value"].is_null())request=nullptr;
        else request=std::make_shared<Json>(op["value"]);
        bootstrap::setLastAPIRequest(state,request);
      }else if(kind=="messages"){
        messagesSet=true;
        if(op["value"].is_null())messages=nullptr;
        else messages=std::make_shared<Json>(op["value"]);
        bootstrap::setLastAPIRequestMessages(state,messages);
      }else if(kind=="classifiers"){
        classifiersSet=true;
        if(op["value"].is_null())classifiers=nullptr;
        else classifiers=std::make_shared<Json>(op["value"]);
        bootstrap::setLastClassifierRequests(state,classifiers);
      }else if(kind=="content"){
        std::optional<std::string> content;
        if(!op["value"].is_null())
          content=op["value"].get<std::string>();
        bootstrap::setStashedBatboxMdContent(state,std::move(content));
      }else if(kind=="mutateRequest")
        (*request)[op["key"].get<std::string>()]=op["value"];
      else if(kind=="appendMessage")messages->push_back(op["value"]);
      else if(kind=="appendClassifier")
        classifiers->push_back(op["value"]);
      else if(kind=="error")
        bootstrap::addToInMemoryErrorLog(state,
          {op["value"]["error"].get<std::string>(),
           op["value"]["timestamp"].get<std::string>()});
      else throw std::runtime_error("Unknown request-state operation");
    }
    Json errors=Json::array();
    for(const auto& entry:state.inMemoryErrorLog)
      errors.push_back({{"error",entry.error},{"timestamp",entry.timestamp}});
    const Json output={{"request",bootstrap::getLastAPIRequest(state)?
        *bootstrap::getLastAPIRequest(state):Json(nullptr)},
      {"sameRequest",requestSet?Json(bootstrap::getLastAPIRequest(state)==
        request):Json(nullptr)},
      {"messages",bootstrap::getLastAPIRequestMessages(state)?
        *bootstrap::getLastAPIRequestMessages(state):Json(nullptr)},
      {"sameMessages",messagesSet?
        Json(bootstrap::getLastAPIRequestMessages(state)==messages):
        Json(nullptr)},
      {"classifiers",bootstrap::getLastClassifierRequests(state)?
        *bootstrap::getLastClassifierRequests(state):Json(nullptr)},
      {"sameClassifiers",classifiersSet?
        Json(bootstrap::getLastClassifierRequests(state)==classifiers):
        Json(nullptr)},
      {"content",bootstrap::getStashedBatboxMdContent(state)?
        Json(*bootstrap::getStashedBatboxMdContent(state)):Json(nullptr)},
      {"errors",errors}};
    std::cout<<value::stringify(output)<<'\n';
  }
}
