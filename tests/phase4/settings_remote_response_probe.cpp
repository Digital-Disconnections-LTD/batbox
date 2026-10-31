#include "settings/remote/ResponseSchema.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto result=settings::remote::parseRemoteManagedSettingsResponse(
      value::parse(line));
    if(result.success)
      std::cout<<value::stringify(Json{{"success",true},{"data",result.data}})
        <<'\n';
    else std::cout<<value::stringify(Json{{"success",false},
      {"errorMessage",result.errorMessage}})<<'\n';
  }
}
