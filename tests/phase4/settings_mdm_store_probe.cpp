#include "settings/mdm/Store.hpp"
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
    event_loop::Loop loop;
    Json trace=Json::array(),values=Json::array();
    persistence::SafeJsonParser safeJson([](const std::string&){});
    PathServices pathServices;
    pathServices.internalUser=true;
    pathServices.managedPathOverride="/managed";
    Paths paths(pathServices);
    batbox::settings::mdm::ParseServices parser;
    parser.paths=&paths;parser.safeJson=&safeJson;
    parser.parser.filterInvalidPermissionRules=[](Json&,const std::string&){
      return Json::array();};
    parser.parser.validateSettingsSchema=[](const Json& data){
      return SchemaResult{true,data,Json::array()};};
    parser.parser.formatZodError=[](const Json&,const std::string&){
      return Json::array();};
    parser.readFileSync=[](const std::string&)->std::string{
      throw std::runtime_error("ENOENT");};
    parser.readdirSync=[](const std::string&)->std::vector<DirEntry>{
      throw std::runtime_error("ENOENT");};
    std::size_t read=0;
    const auto nextRaw=[&](){
      RawReadResult result;
      if(test.contains("raws")&&read<test["raws"].size()){
        const auto& source=test["raws"][read++];
        if(source.contains("plistStdouts")){
          result.plistStdouts=std::vector<PlistStdout>{};
          for(const auto& item:source["plistStdouts"])
            result.plistStdouts->push_back({item.value("stdout",std::string{}),
              item.value("label",std::string{})});
        }
        if(source.contains("hklmStdout"))
          result.hklmStdout=source["hklmStdout"].get<std::string>();
        if(source.contains("hkcuStdout"))
          result.hkcuStdout=source["hkcuStdout"].get<std::string>();
      }
      return result;
    };
    StoreServices services;
    services.startupRaw=[&]()->std::optional<
        event_loop::Deferred<RawReadResult>>{
      trace.push_back({"startupRaw"});
      if(!test.value("startupRaw",false))return std::nullopt;
      event_loop::Deferred<RawReadResult> output(loop);
      output.resolve(nextRaw());return output;
    };
    services.freshRaw=[&](){
      trace.push_back({"freshRaw"});
      event_loop::Deferred<RawReadResult> output(loop);
      output.resolve(nextRaw());return output;
    };
    services.profileCheckpoint=[&](const std::string& value){
      trace.push_back({"profile",value});};
    double clock=100;
    services.nowMs=[&](){const auto value=clock;clock+=23;return value;};
    services.debug=[&](const std::string& value){
      trace.push_back({"debug",value});};
    services.localDiagnostics=[&](const std::string& level,
        const std::string& name,const Json& data){
      trace.push_back({"diagnostic",level,name,data});
      if(test.value("diagnosticError",false))
        throw std::runtime_error("diagnostic failed");
    };
    Store store(loop,parser,services);
    const auto present=[](const MdmResult& result){
      return Json{{"settings",result.settings},{"errors",result.errors}};
    };
    for(const auto& action:test["actions"]){
      const auto op=action.get<std::string>();
      if(op=="start"){
        store.startMdmSettingsLoad();loop.runReady();
      }else if(op=="ensure"){
        bool done=false;
        store.ensureMdmSettingsLoaded().then([&](
            const event_loop::Unit*,const std::exception_ptr& error){
          if(error)std::rethrow_exception(error);
          done=true;
        });
        for(int n=0;n<100&&!done;n++)loop.runReady();
        if(!done)throw std::runtime_error("MDM ensure did not settle");
      }else if(op=="refresh"){
        bool done=false;
        store.refreshMdmSettings().then([&](const ParsedRaw* result,
            const std::exception_ptr& error){
          if(error)std::rethrow_exception(error);
          values.push_back(Json{{"mdm",present(result->mdm)},
            {"hkcu",present(result->hkcu)}});
          done=true;
        });
        for(int n=0;n<100&&!done;n++)loop.runReady();
        if(!done)throw std::runtime_error("MDM refresh did not settle");
      }else if(op=="clear")store.clearMdmSettingsStash();
      else if(op=="set"){
        const auto& mdm=test["set"]["mdm"];
        const auto& hkcu=test["set"]["hkcu"];
        store.setMdmSettingsStash({mdm["settings"],mdm["errors"]},
          {hkcu["settings"],hkcu["errors"]});
      }else if(op=="read")values.push_back(Json{
        {"mdm",present(store.getMdmSettings())},
        {"hkcu",present(store.getHkcuSettings())}});
    }
    std::cout<<value::stringify(Json{{"values",values},{"trace",trace}})<<'\n';
  }
}
