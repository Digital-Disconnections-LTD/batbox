#include "auth/storage/MacKeychain.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    auth::storage::KeychainLockDetector detector(
      [&](){return test.value("platform",std::string("linux"));},
      [&](){trace.push_back(Json::array({"security",
        Json::array({"show-keychain-info"})}));
        if(test.value("error",false))
          throw std::runtime_error("security failed");
        return auth::storage::SecurityResult{
          test.value("exitCode",0),{}};});
    std::cout<<value::stringify(Json{{"values",Json::array({
      detector.isLocked(),detector.isLocked()})},{"trace",trace}})<<'\n';
  }
}
