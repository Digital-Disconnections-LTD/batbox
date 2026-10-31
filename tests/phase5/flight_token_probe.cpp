#include "cli/remote/FlightToken.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::cli::remote;
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto token=value::parse(line).get<std::string>();
    const auto expiry=decodeFlightJwtExpiry(token,"sk-ant-si-");
    std::cout<<value::stringify(Json{{"payload",decodeFlightJwtPayload(
      token,"sk-ant-si-")},{"expiry",expiry?Json(*expiry):Json(nullptr)}})
      <<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
