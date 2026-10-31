#include "gadgets/synthetic_output/SyntheticOutputFactory.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json trace=Json::array();
  gadgets::synthetic_output::AjvBackend backend;
  backend.validateSchema=[&](const Json& schema)->std::optional<std::string>{
    trace.push_back("new:true");
    const auto name=schema.at("name").get<std::string>();
    trace.push_back("schema:"+name);
    if(schema.value("invalid",false)){
      trace.push_back("errorsText");return "schema invalid";
    }
    return {};
  };
  backend.compile=[&](const Json& schema)->gadgets::synthetic_output::AjvValidator{
    const auto name=schema.at("name").get<std::string>();
    trace.push_back("compile:"+name);
    if(schema.value("throwCompile",false))
      throw std::runtime_error("compile bad");
    if(schema.value("throwString",false))
      throw std::runtime_error("string compile bad");
    return [&,name](const Json& input){
      trace.push_back("input:"+name);
      if(input.value("reject",false))return gadgets::synthetic_output::AjvValidation{
        false,std::vector<gadgets::synthetic_output::AjvValidationIssue>{
          {"","must pass"},{"/x","is wrong"}}};
      return gadgets::synthetic_output::AjvValidation{true,{}};
    };
  };
  gadgets::synthetic_output::SyntheticOutputFactory factory(
    gadgets::synthetic_output::makeSyntheticOutputGadget("StructuredOutput"),
    std::move(backend));
  auto schema=std::make_shared<const Json>(Json{{"name","A"}});
  auto first=factory.create(schema),second=factory.create(schema);
  auto equal=factory.create(std::make_shared<const Json>(Json{{"name","A"}}));
  auto invalid=factory.create(std::make_shared<const Json>(
    Json{{"name","B"},{"invalid",true}}));
  auto invalidAgain=factory.create(std::make_shared<const Json>(
    Json{{"name","B"},{"invalid",true}}));
  auto compileError=factory.create(std::make_shared<const Json>(
    Json{{"name","C"},{"throwCompile",true}}));
  auto stringError=factory.create(std::make_shared<const Json>(
    Json{{"name","D"},{"throwString",true}}));
  const auto validResult=first->gadget->call(Json{{"ok",true}}, {}, {},{},{}).get();
  Json valid={{"data",validResult.data},
    {"structured_output",*validResult.structuredOutput}};
  std::string rejected;
  try{first->gadget->call(Json{{"reject",true}}, {}, {},{},{}).get();}
  catch(const std::exception& error){rejected=error.what();}
  Json row={{"same",first==second},{"different",first!=equal},
    {"invalid",*invalid->error},{"invalidDifferent",invalid!=invalidAgain},
    {"compileError",*compileError->error},
    {"stringError",*stringError->error},
    {"schema",first->gadget->inputJSONSchema},{"valid",valid},
    {"rejected",rejected},{"trace",trace}};
  std::cout<<row.dump()<<'\n';
}
