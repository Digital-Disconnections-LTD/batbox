#include "cli/structured/AsyncStream.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array();
    cli::structured::AsyncStream<Json> stream(loop,[&]{
      events.push_back(Json::array({"returned"}));
    });
    for(const auto& action:input.at("actions")){
      const auto kind=action.at(0).get<std::string>();
      if(kind=="iterate"){
        try{stream.iterate();events.push_back(Json::array({"iterate"}));}
        catch(const std::exception& error){
          events.push_back(Json::array({"throw",error.what()}));
        }
      }else if(kind=="next"){
        const auto index=action.at(1);
        stream.next().then([&,index](
            const cli::structured::StreamItem<Json>* value,
            const std::exception_ptr& error){
          if(error){
            try{std::rethrow_exception(error);}
            catch(const std::exception& failure){
              events.push_back(Json::array({"reject",index,failure.what()}));
            }
          }else{
            Json item={{"done",value->done}};
            if(value->value)item["value"]=*value->value;
            events.push_back(Json::array({"next",index,item}));
          }
        });
      }else if(kind=="enqueue")stream.enqueue(action.at(1));
      else if(kind=="done")stream.done();
      else if(kind=="error")stream.error(std::make_exception_ptr(
        std::runtime_error(action.at(1).get<std::string>())));
      else if(kind=="return")stream.returnStream().then([&](
          const cli::structured::StreamItem<Json>* value,
          const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        Json item={{"done",value->done}};
        if(value->value)item["value"]=*value->value;
        events.push_back(Json::array({"return",item}));
      });
    }
    const Json before=events;
    loop.runReady();
    std::cout<<value::stringify(Json{{"before",before},{"after",events}})<<'\n';
  }
}
