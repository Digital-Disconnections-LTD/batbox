#include "gadgets/bash/Schema.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  using namespace batbox;
  Json inputs=value::parse(R"([{"command":"echo hello"},{"command":"sleep 1","timeout":"30","run_in_background":"true","dangerouslyDisableSandbox":"false"},{"command":"","timeout":"-5.25","description":"test"},{"command":"x","timeout":"0"},{"command":"x","timeout":"1e2"},{"command":"x","timeout":""},{"command":"x","timeout":"NaN"},{"command":"x","timeout":null},{"command":"x","timeout":true},{"command":"x","run_in_background":"TRUE"},{"command":"x","run_in_background":0},{"command":"x","dangerouslyDisableSandbox":"false"},{"command":"x","_simulatedSedEdit":{"filePath":"a","newContent":"b"}},{"command":"x","_simulatedSedEdit":{"filePath":"a","newContent":"b","ignored":true}},{"command":"x","unknown":true},{"command":1},{"description":"missing"},[],null,{"command":"x","timeout":"9999999999999999999999999999999999999999999999999"},{"command":"x","timeout":"0001.5"},{"command":"x","timeout":"-0"}])");
  Json cases=Json::array();
  for(const auto& input:inputs){
    Json model,full;
    try{model={{"success",true},{"data",bash_gadget::parseBashInput(input)}};}
    catch(...){model={{"success",false}};}
    try{full={{"success",true},{"data",bash_gadget::parseBashInput(input,false,true)}};}
    catch(...){full={{"success",false}};}
    cases.push_back({{"input",input},{"model",model},{"full",full}});
  }
  Json output={{"schema",{{"input",bash_gadget::bashInputJSONSchema(600000)},
    {"output",bash_gadget::bashOutputJSONSchema()},
    {"disabledInput",bash_gadget::bashInputJSONSchema(600000,true)}}},{"cases",cases}};
  std::cout<<value::stringify(output)<<'\n';
}
