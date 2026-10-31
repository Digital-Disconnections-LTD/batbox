#include "settings/mdm/AppBindings.hpp"
#include <chrono>
#include <utility>

namespace batbox::settings::mdm {
ParseServices bindIndigenousMdmParser(Paths& paths,
    persistence::SafeJsonParser& safeJson,
    const schema::MainConfig& config,const PermissionRuleServices& rules,
    const std::string& docsBase,Names names){
  return indigenousParseServices(paths,safeJson,
    schema::bindIndigenousSettingsParser(config,rules,docsBase),
    std::move(names));
}
StoreServices bindRawReader(RawReader& reader,
    std::function<void(const std::string&)> profileCheckpoint,
    std::function<void(const std::string&)> debug,
    std::function<void(const std::string&,const std::string&,const Json&)>
      localDiagnostics){
  StoreServices services;
  services.startupRaw=[&reader](){return reader.getMdmRawReadPromise();};
  services.freshRaw=[&reader](){return reader.fireRawRead();};
  services.profileCheckpoint=std::move(profileCheckpoint);
  services.nowMs=[](){
    const auto now=std::chrono::system_clock::now().time_since_epoch();
    return static_cast<double>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  };
  services.debug=std::move(debug);
  services.localDiagnostics=std::move(localDiagnostics);
  return services;
}
void bindSettingsConsumers(Store& store,LoadServices& loader,
    persistence::MdmPollServices& poll){
  loader.getMdmSettings=[&store](){
    const auto result=store.getMdmSettings();
    return ParsedSettings{result.settings,result.errors};
  };
  loader.getHkcuSettings=[&store](){
    const auto result=store.getHkcuSettings();
    return ParsedSettings{result.settings,result.errors};
  };
  poll.getMdmSettings=[&store](){return store.getMdmSettings();};
  poll.getHkcuSettings=[&store](){return store.getHkcuSettings();};
  poll.refreshMdmSettings=[&store](auto done){
    store.refreshMdmSettings().then([done=std::move(done)](
        const ParsedRaw* result,const std::exception_ptr& error){
      if(error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& caught){done(std::nullopt,caught.what());}
        catch(...){done(std::nullopt,"Unknown MDM read error");}
        return;
      }
      done(persistence::MdmValues{result->mdm,result->hkcu},std::nullopt);
    });
  };
  poll.setMdmSettingsStash=[&store](const persistence::MdmValues& values){
    store.setMdmSettingsStash(values.mdm,values.hkcu);
  };
}
}
