#include "auth/refresh/ProfileHttp.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array();
    auth::refresh::ProfileHttpServices services;
    services.baseApiUrl=[&](){trace.push_back("base");
      return test.value("baseUrl",std::string("https://auth.batbox.test"));};
    services.httpGet=[&](const std::string& url,const Json& headers,
        double timeout){
      trace.push_back(Json::array({"get",url,Json{{"headers",headers},
        {"timeout",timeout}}}));
      event_loop::Deferred<auth::refresh::HttpResponse> output(loop);
      if(test.value("fail",false))output.reject(
        std::make_exception_ptr(std::runtime_error("profile failed")));
      else output.resolve({200,"OK",test.value("data",Json(nullptr))});
      return output;
    };
    services.logError=[&](const std::string& message){
      trace.push_back(Json::array({"error",message}));};
    auto result=auth::refresh::getRawOAuthProfile(loop,
      test.value("token",std::string("access")),services);
    bool done=false;
    Json value=nullptr;
    result.then([&](const std::optional<Json>* output,
        const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      if(*output)value=**output;
      done=true;
    });
    for(int attempt=0;attempt<100&&!done;++attempt)loop.runReady();
    if(!done)throw std::runtime_error("Profile HTTP did not settle");
    std::cout<<value::stringify(Json{{"value",value},{"trace",trace}})
      <<'\n';
  }
}
