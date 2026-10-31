#include "diagnostics/ExecSyncWrapper.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace batbox;
int main(){
  const std::string raw(std::istreambuf_iterator<char>(std::cin),{});
  const auto input=value::parse(raw);
  Json calls=Json::array(),result=nullptr;
  std::string status="fulfilled";
  diagnostics::ExecSyncImports imports;
  imports.beginSlowLogging=[&](const std::vector<std::string>& strings,
      const std::vector<std::optional<Json>>& values){
    Json mapped=Json::array();
    for(const auto& value:values)mapped.push_back(value?*value:Json(nullptr));
    calls.push_back(Json::array({"begin",strings,mapped}));
    return std::function<void()>([&]{
      calls.push_back(Json::array({"dispose"}));
      if(input.value("disposeThrow",false))throw std::runtime_error("dispose failed");
    });
  };
  imports.nodeExecSync=[&](const std::string& command,const std::optional<Json>& options){
    calls.push_back(Json::array({"exec",command,options?*options:Json(nullptr)}));
    if(input.value("execThrow",false))throw std::runtime_error("exec failed");
    auto text=input.value("returnValue",std::string("ok"));
    if(text.empty())text="ok";
    if(input.value("returnBytes",false))return diagnostics::ExecSyncResult(
      std::vector<unsigned char>(text.begin(),text.end()));
    return diagnostics::ExecSyncResult(text);
  };
  try{
    std::optional<Json> options;
    if(input.contains("options"))options=input["options"];
    auto value=diagnostics::execSyncDeprecated(input.at("command"),options,imports);
    if(std::holds_alternative<std::string>(value))result=std::get<std::string>(value);
    else result=Json::object({{"type","Buffer"},
      {"data",std::get<std::vector<unsigned char>>(value)}});
  }catch(const std::exception& error){status="rejected";result=error.what();}
  std::cout<<value::stringify(Json::object({{"status",status},{"result",result},
    {"calls",calls}}));
}
