#include "settings/mdm/Parse.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  using namespace batbox::settings;
  using namespace batbox::settings::mdm;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    persistence::SafeJsonParser safeJson([](const std::string&){});
    PathServices pathServices;
    pathServices.internalUser=true;
    pathServices.managedPathOverride="/managed";
    Paths paths(pathServices);
    batbox::settings::mdm::ParseServices services;
    services.paths=&paths;
    services.safeJson=&safeJson;
    services.parser.filterInvalidPermissionRules=[&](Json& data,
        const std::string& source){
      trace.push_back({"filter",source});
      if(data.is_object()&&data.contains("invalidRule")&&
          value::truthy(data["invalidRule"])){
        data.erase("invalidRule");
        return Json::array({Json{{"file",source},
          {"message","invalid rule"}}});
      }
      return Json::array();
    };
    services.parser.validateSettingsSchema=[&](const Json& data){
      trace.push_back({"schema",data});
      if(data.is_object()&&data.contains("reject")&&
          value::truthy(data["reject"]))
        return SchemaResult{false,nullptr,Json::array({"bad schema"})};
      return SchemaResult{true,data,Json::array()};
    };
    services.parser.formatZodError=[&](const Json& issues,
        const std::string& source){
      trace.push_back({"format",source});
      Json errors=Json::array();
      for(const auto& message:issues)
        errors.push_back(Json{{"file",source},{"message",message}});
      return errors;
    };
    const auto files=test.value("files",Json::object());
    services.readFileSync=[&](const std::string& file){
      trace.push_back({"read",file});
      if(!files.contains(file))throw std::runtime_error("ENOENT");
      return files[file].get<std::string>();
    };
    services.readdirSync=[&](const std::string& directory){
      trace.push_back({"readdir",directory});
      if(test.value("dirError",false))throw std::runtime_error("ENOENT");
      std::vector<DirEntry> result;
      for(const auto& item:test.value("entries",Json::array()))
        result.push_back({item.value("name",std::string{}),
          item.value("file",false),item.value("symlink",false)});
      return result;
    };
    Json value;
    const auto op=test.value("op",std::string{});
    if(op=="registry"){
      const auto parsed=parseRegQueryStdout(
        test["stdout"].get<std::string>(),
        test.value("valueName",std::string("Settings")));
      value=parsed?Json(*parsed):Json(nullptr);
    }else if(op=="command"){
      const auto result=parseCommandOutputAsSettings(
        test["stdout"].get<std::string>(),
        test["source"].get<std::string>(),services);
      value=Json{{"settings",result.settings},{"errors",result.errors}};
    }else if(op=="managed")value=hasManagedSettingsFile(services);
    else{
      RawReadResult raw;
      if(test.contains("raw")){
        const auto& input=test["raw"];
        if(input.contains("plistStdouts")&&!input["plistStdouts"].is_null()){
          raw.plistStdouts=std::vector<PlistStdout>{};
          for(const auto& item:input["plistStdouts"])
            raw.plistStdouts->push_back({item.value("stdout",std::string{}),
              item.value("label",std::string{})});
        }
        if(input.contains("hklmStdout")&&!input["hklmStdout"].is_null())
          raw.hklmStdout=input["hklmStdout"].get<std::string>();
        if(input.contains("hkcuStdout")&&!input["hkcuStdout"].is_null())
          raw.hkcuStdout=input["hkcuStdout"].get<std::string>();
      }
      const auto result=consumeRawReadResult(raw,services);
      value=Json{{"mdm",Json{{"settings",result.mdm.settings},
          {"errors",result.mdm.errors}}},
        {"hkcu",Json{{"settings",result.hkcu.settings},
          {"errors",result.hkcu.errors}}}};
    }
    std::cout<<value::stringify(Json{{"value",value},{"trace",trace}})<<'\n';
  }
}
