#include "diagnostics/SlowOperations.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace batbox;

int main(){
  const std::string raw(std::istreambuf_iterator<char>(std::cin),{});
  const auto input=value::parse(raw);
  std::size_t nowIndex=0,stackCalls=0;
  bool didReenter=false;
  Json logs=Json::array(),slow=Json::array(),fileCalls=Json::array();
  diagnostics::SlowOperationConfig config;
  config.featureEnabled=input.value("feature",false);
  if(input.contains("threshold")&&!input["threshold"].is_null())
    config.thresholdEnvironment=input["threshold"].get<std::string>();
  config.nodeEnvironment=input.value("nodeEnv",std::string());
  config.internalUser=input.value("internal",false);
  diagnostics::SlowOperationImports imports;
  diagnostics::SlowOperations* active=nullptr;
  imports.performanceNow=[&]{
    return input.at("times").at(nowIndex++).get<double>();};
  imports.captureStack=[&]() -> std::optional<std::string>{
    ++stackCalls;
    if(!input.contains("stack")||input["stack"].is_null())return std::nullopt;
    return input["stack"].get<std::string>();
  };
  imports.logForDebugging=[&](const std::string& message){
    logs.push_back(message);
    if(input.value("reenter",false)&&!didReenter){
      didReenter=true;active->jsonParse("{\"nested\":true}");
    }
    if(input.value("failDebug",false))throw std::runtime_error("logger failed");
  };
  imports.addSlowOperation=[&](const std::string& description,double duration){
    slow.push_back(Json::array({description,duration}));
    if(input.value("failAdd",false))throw std::runtime_error("slow sink failed");
  };
  imports.parseJson=[](const std::string& text){return value::parse(text);};
  imports.stringifyJson=[](const Json& value,const std::optional<Json>& replacer,
      const std::optional<Json>& space){
    if(replacer&&!replacer->is_null())
      throw std::runtime_error("Replacer import is required");
    if(space&&space->is_number())return value::stringify(value,
      std::min(10,std::max(0,space->get<int>())));
    return value::stringify(value);
  };
  imports.structuredCloneJson=[](const Json& value,const std::optional<Json>&){return value;};
  imports.lodashCloneDeepJson=[](const Json& value){return value;};
  auto fileCall=[&](const std::string& name,Json arguments){
    Json call=Json::array({name});
    for(auto& argument:arguments)call.push_back(argument);
    fileCalls.push_back(std::move(call));
    if(input.value("failAt",std::string())==name)
      throw std::runtime_error(name+" failed");
  };
  imports.openSync=[&](const std::string& path,const std::optional<Json>& mode){
    fileCall("open",Json::array({path,"w",mode?*mode:Json(nullptr)}));return 7;};
  imports.writeFdSync=[&](int fd,const diagnostics::SlowWriteData& data,
      const std::optional<Json>& encoding){
    const auto text=std::holds_alternative<std::string>(data)?
      std::get<std::string>(data):
      std::string(std::get<std::vector<unsigned char>>(data).begin(),
        std::get<std::vector<unsigned char>>(data).end());
    Json options=Json::object();if(encoding)options["encoding"]=*encoding;
    fileCall("writeFd",Json::array({fd,text,options}));
  };
  imports.fsyncSync=[&](int fd){fileCall("fsync",Json::array({fd}));};
  imports.closeSync=[&](int fd){fileCall("close",Json::array({fd}));};
  imports.writePathSync=[&](const std::string& path,
      const diagnostics::SlowWriteData& data,const std::optional<Json>& options){
    const auto text=std::holds_alternative<std::string>(data)?
      std::get<std::string>(data):
      std::string(std::get<std::vector<unsigned char>>(data).begin(),
        std::get<std::vector<unsigned char>>(data).end());
    fileCall("writePath",Json::array({path,text,options?*options:Json(nullptr)}));
  };
  diagnostics::SlowOperations operations(config,imports);
  active=&operations;
  Json result=nullptr;
  std::string status="fulfilled";
  try{
    const auto op=input.value("operation",std::string("parse"));
    if(op=="frame"){
      std::optional<std::string> stack;
      if(input.contains("stack")&&!input["stack"].is_null())
        stack=input["stack"].get<std::string>();
      result=diagnostics::slowOperationCallerFrame(stack);
    }else if(op=="description"){
      std::vector<std::string> strings=input.at("strings").get<std::vector<std::string>>();
      std::vector<std::optional<Json>> values;
      for(const auto& value:input.at("values")){
        if(value.is_object()&&value.value("$undefined",false))values.emplace_back();
        else values.push_back(value);
      }
      result=diagnostics::buildSlowOperationDescription(strings,values);
    }else if(op=="threshold"){
      const auto value=operations.threshold();
      result=std::isfinite(value)?Json(value):Json("Infinity");
    }else if(op=="stringify"){
      std::optional<Json> replacer,space;
      if(input.contains("replacer"))replacer=input["replacer"];
      if(input.contains("space"))space=input["space"];
      result=operations.jsonStringify(input.at("value"),replacer,space);
    }else if(op=="clone"){
      std::optional<Json> options;
      if(input.contains("options"))options=input["options"];
      result=operations.clone(input.at("value"),options);
    }else if(op=="cloneDeep")result=operations.cloneDeep(input.at("value"));
    else if(op=="write"){
      diagnostics::SlowWriteData data=input.at("value").get<std::string>();
      if(input.value("buffer",false)){
        const auto text=input.at("value").get<std::string>();
        data=std::vector<unsigned char>(text.begin(),text.end());
      }
      std::optional<Json> options;
      if(input.contains("options"))options=input["options"];
      operations.writeFileSyncDeprecated(input.at("path"),data,options);
      result=nullptr;
    }
    else if(op=="scope"){
      auto value=std::make_shared<Json>(input.at("value"));
      std::vector<diagnostics::SlowArgument> values;
      values.emplace_back(value);
      auto dispose=operations.beginSlowLogging(
        input.at("strings").get<std::vector<std::string>>(),std::move(values));
      if(input.value("mutate",false))(*value)["extra"]="later";
      dispose();
      if(input.value("disposeTwice",false))dispose();
      result=nullptr;
    }
    else result=operations.jsonParse(input.at("value").get<std::string>());
  }catch(const diagnostics::SuppressedOperationError& error){
    status="rejected";result=error.what();
  }catch(const nlohmann::json::parse_error&){
    status="rejected";result="parseError";
  }catch(const std::exception& error){
    status="rejected";result=error.what();
  }
  std::cout<<value::stringify(Json::object({{"status",status},{"result",result},
    {"logs",logs},{"slow",slow},{"fileCalls",fileCalls},{"nowCalls",nowIndex},
    {"stackCalls",stackCalls}}));
}
