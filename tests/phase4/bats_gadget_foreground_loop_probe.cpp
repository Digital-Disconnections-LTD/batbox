#define main batGadgetForegroundFixtureMain
#include "bats_gadget_foreground_indigenous.cpp"
#undef main
#include <string>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json item=batbox::value::parse(line);
    Fixture fixture;
    Json trace=Json::array(),captured=Json::array();
    fixture.services.isBackgroundMissionsDisabled=[](){return true;};
    fixture.plan.runBatParams.poetryMessages=Json::array();
    fixture.services.progress.onProgress=[&](const Json& event){trace.push_back(event);};
    fixture.services.progress.getAssistantMessageContentLength=[](const Json& message){
      double length=0;
      for(const auto& block:field(field(message,"message"),"content"))
        if(field(block,"type")=="text")
          length+=field(block,"text").get<std::string>().size();
      return length;
    };
    fixture.services.final.result.getLastAssistantMessage=[&](const Json& messages){
      captured=messages;
      for(auto it=messages.rbegin();it!=messages.rend();++it)
        if(field(*it,"type")=="assistant")return *it;
      return Json();
    };
    fixture.time=item.at("nowMillis").get<std::int64_t>();
    fixture.services.runBat=[item](const batbox::bats::RunBatParams&,
        const batbox::query::Yield& yield){
      std::vector<batbox::query::MessageRef> refs;
      for(const auto& message:item.at("messages")){
        auto ref=std::make_shared<Json>(message);refs.push_back(ref);yield(ref);
      }
      if(item.contains("lateUsage"))for(auto it=refs.rbegin();it!=refs.rend();++it){
        if(field(**it,"type")!="assistant")continue;
        batbox::runtime::MessageLock lock(*it);
        lock.value()["message"]["usage"]["output_tokens"]=item.at("lateUsage");
        break;
      }
    };
    try{
      fixture.run();
      std::cout<<batbox::value::stringify(Json{{"messages",captured},
        {"responseLength",fixture.responseLength},{"trace",trace}})<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
    }
  }
}
