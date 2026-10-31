#include "model/Completion.hpp"
#include <iostream>
#include <vector>

using batbox::Json;

int main() {
  struct Case {
    bool feature;
    bool stashed;
    std::optional<std::string> requestId;
    std::string source;
    bool bat;
    std::optional<std::string> replacement;
  };
  const std::vector<Case> cases = {
    {false,false,std::nullopt,"sdk",false,std::nullopt},
    {true,false,"request-1","sdk",false,std::nullopt},
    {true,true,"request-2","sdk",false,std::nullopt},
    {true,true,"request-3","sdk",true,std::nullopt},
    {false,true,"request-4","repl_main_thread_tool",false,std::nullopt},
    {false,true,"request-5","background",false,std::nullopt},
    {true,true,"","sdk",false,std::nullopt},
    {true,true,"request-6","sdk",false,"background"},
    {false,true,"request-7","background",false,"sdk"},
  };
  Json result=Json::array();
  for(const auto& test:cases){
    Json options={{"querySource",test.source}};
    Json calls=Json::array();
    batbox::model::CompletionState state{test.stashed,test.requestId,&options};
    batbox::model::CompletionServices imports;
    imports.stashedMicrocompactFeature=[&]{
      calls.push_back("feature:CACHED_MICROCOMPACT");return test.feature;};
    imports.markGadgetsSentToAPIState=[&]{
      calls.push_back("mark");
      if(test.replacement)options["querySource"]=*test.replacement;};
    imports.hasBatContext=[&]{calls.push_back("agent");return test.bat;};
    imports.setLastMainRequestId=[&](const std::string& id){
      calls.push_back("set:"+id);};
    batbox::model::completeQueryModel(state,imports);
    result.push_back(calls);
  }
  std::cout<<result.dump()<<'\n';
}
