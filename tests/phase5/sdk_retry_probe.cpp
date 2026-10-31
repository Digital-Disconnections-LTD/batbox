#include "transport/SdkRetry.hpp"
#include <iostream>
#include <limits>
using namespace batbox;
Json run(const Json& c){
  const auto headers=c.value("headers",Json::object());
  if(c.value("op","")=="policy"){
    transport::SdkRetryAuth auth;int refreshed=0;
    if(c.value("auth",false)){auth.usedTokenStash=true;auth.invalidateTokenStash=[&]{++refreshed;};}
    Json decisions=Json::array();
    for(const auto& status:c.at("statuses"))decisions.push_back(
      transport::sdkShouldRetry(status.get<long>(),headers,auth));
    return {{"decisions",decisions},{"refreshes",refreshed}};
  }
  transport::SdkRetryClock clock{
    [&]{return c.value("random",0.5);},[&]{return c.value("now",1000.);},
    [&](const std::string&){return c.value("date",std::numeric_limits<double>::quiet_NaN());}};
  const int remaining=c.value("remaining",2);
  return {{"delay",transport::sdkRetryDelay(headers,remaining,c.value("max",2),clock)},
    {"remaining",remaining-1}};
}
int main(){for(std::string line;std::getline(std::cin,line);)std::cout<<run(Json::parse(line))<<'\n';}
