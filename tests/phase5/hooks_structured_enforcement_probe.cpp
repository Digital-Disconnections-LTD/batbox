#include "instructions/hooks/runtime/StructuredOutputEnforcement.hpp"
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);batbox::Json cases;file>>cases;
  batbox::Json rows=batbox::Json::array();
  for(const auto& item:cases)
    rows.push_back({{"name",item.at("name")},{"value",
      batbox::instructions::hooks::runtime::hasSuccessfulGadgetCall(
        item.at("messages"),"StructuredOutput")}});
  std::cout<<rows.dump()<<'\n';
  std::string event,matcher,error;double timeout=0;
  batbox::instructions::hooks::runtime::StructuredOutputCheck check;
  batbox::instructions::hooks::runtime::registerStructuredOutputEnforcement(
    "session","StructuredOutput",[&](const std::string& flight,
      const std::string& e,const std::string& m,
      batbox::instructions::hooks::runtime::StructuredOutputCheck callback,
      const std::string& message,double ms){
      assert(flight=="session");event=e;matcher=m;check=std::move(callback);
      error=message;timeout=ms;
    });
  assert(event=="Stop"&&matcher.empty()&&timeout==5000);
  assert(error=="You MUST call the StructuredOutput tool to complete this request. Call this tool now.");
  assert(check(cases.at(0).at("messages")));
}
