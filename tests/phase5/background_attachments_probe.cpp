#include "interactive/background/NotificationAttachments.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive::background;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  const std::vector<Json> cases={
    Json::array({
      Json{{"mode","task-notification"},{"value","notice"}},
      Json{{"mode","prompt"},{"value",Json::array({
        Json{{"type","text"},{"text",""}},
        Json{{"type","text"},{"text","tail"}}})},
        {"pastedContents",{{"2",{{"type","image"},{"id",2},
          {"content","base64"},{"mediaType",""}}},
          {"1",{{"type","image"},{"id",1},{"content",""}}}}}},
      Json{{"mode","bash"},{"value","skip"}}}),
    Json::array({Json{{"mode","prompt"},{"value","image"},
      {"uuid","u"},{"origin","user"},{"isMeta",false},
      {"pastedContents",{{"3",{{"type","image"},{"id",3},
        {"content","c3"},{"mediaType","image/jpeg"}}},
        {"2",{{"type","text"},{"id",2},{"content","text"}}}}}}}),
    Json::array({Json{{"mode","prompt"},{"value","array"},
      {"pastedContents",Json::array({Json{{"type","image"},
        {"id",4},{"content","a"}}})}}}),
    Json::array({Json{{"mode","prompt"},{"value","bad"},
      {"pastedContents",{{"1",nullptr}}}}})
  };
  for(const auto& input:cases){
    event_loop::Loop loop;
    interactive::submit::CommandList commands;
    for(const auto& command:input)
      commands.push_back(std::make_shared<Json>(command));
    AttachmentServices imports;
    imports.maybeResizeAndDownsampleImageBlock=[&](const Json& block){
      event_loop::Deferred<Json> value(loop);
      Json copy=block;copy["resized"]=true;value.resolve(std::move(copy));
      return value;
    };
    auto result=getQueuedCommandAttachments(loop,commands,imports);
    Json output;
    result.then([&](const Json* value,const std::exception_ptr& error){
      if(error){output=Json{{"rejected",true}};return;}
      if(value)output=*value;
    });
    for(int i=0;i<10&&output.is_null();++i)loop.runReady();
    if(output.is_null())throw std::runtime_error("attachments did not settle");
    std::cout<<output.dump()<<'\n';
  }
}
