#include "gadgets/Schema.hpp"
#include <iostream>
#include <map>
#include <memory>
#include <string>

using batbox::Json;
namespace gadgets=batbox::gadgets;
static gadgets::GadgetSchemaStash stash;
static gadgets::ZodSchemaStash zodStash;
static std::map<std::string,std::unique_ptr<int>> zodIdentities;
static Json evaluate(const Json& t){
  if(t.at("op")=="clear"){stash.clear();return Json{{"cleared",true}};}
  if(t.at("op")=="zod"){
    const std::string id=t.at("id").get<std::string>();
    if(!zodIdentities.contains(id))zodIdentities[id]=std::make_unique<int>(0);
    int calls=0;
    const Json result=zodStash.toJsonSchema(zodIdentities.at(id).get(),[&](){++calls;return t.at("zodOutput");});
    return Json{{"result",result},{"zodConvertCalls",calls}};
  }
  int poetryCalls=0,convertCalls=0,strictCalls=0,debugCalls=0;
  gadgets::Gadget gadget;gadget.name=t.at("tool").get<std::string>();
  gadget.inputSchema=t.value("inputSchema",Json{{"type","object"},{"properties",Json::object()}});
  gadget.inputJSONSchema=t.value("inputJSONSchema",Json());
  gadget.strict=t.value("strict",false);
  gadget.poetry=[&](const gadgets::PoetryContext&){++poetryCalls;std::promise<std::string> p;
    p.set_value(t.value("promptText",std::string("Prompt")));return p.get_future();};
  gadgets::GadgetSchemaOptions options;
  if(t.contains("model")&&t.at("model").is_string())options.model=t.at("model").get<std::string>();
  options.deferLoading=t.value("deferLoading",false);
  options.stashControl=t.value("cacheControl",Json());
  gadgets::GadgetSchemaServices s;
  s.jsonStringify=[](const Json& v){return batbox::value::stringify(v);};
  s.convertInputSchema=[&](const gadgets::Gadget& gadget){++convertCalls;return t.value("convertedSchema",gadget.inputSchema);};
  s.strictGadgetsEnabled=[&](){++strictCalls;return t.value("strictFeature",false);};
  s.batSwarmsEnabled=[&](){return t.value("swarmEnabled",false);};
  s.modelSupportsStructuredOutputs=[&](const std::string&){return t.value("modelSupport",false);};
  s.directProvider=[&](){return t.value("directProvider",false);};
  s.directBackendBaseUrl=[&](){return t.value("directBase",false);};
  s.fineGrainedFeature=[&](){return t.value("fgtsFeature",false);};
  s.fineGrainedEnv=[&](){return t.value("fgtsEnv",false);};
  s.logDebug=[&](const std::string&){++debugCalls;};
  s.disableExperimentalBetas=t.value("disableExperimental",false);
  s.exitPlanModeGadgetName="ExitPlanMode";s.batGadgetName="Agent";
  s.wire={"name","description","input_schema","strict","eager_input_streaming","defer_loading","cache_control"};
  const Json result=gadgets::gadgetToAPISchema(gadget,options,stash,s);
  return Json{{"result",result},{"promptCalls",poetryCalls},{"convertCalls",convertCalls},
    {"strictCalls",strictCalls},{"debugCalls",debugCalls}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<batbox::value::stringify(evaluate(batbox::value::parse(line)))<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
