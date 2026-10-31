#pragma once
#include "settings/Loader.hpp"
#include "settings/mdm/Store.hpp"
#include "settings/persistence/MdmPoll.hpp"
#include "settings/schema/IndigenousBindings.hpp"

namespace batbox::settings::mdm {
ParseServices bindIndigenousMdmParser(Paths& paths,
  persistence::SafeJsonParser& safeJson,
  const schema::MainConfig& config,const PermissionRuleServices& rules,
  const std::string& docsBase,Names names={});
StoreServices bindRawReader(RawReader& reader,
  std::function<void(const std::string&)> profileCheckpoint,
  std::function<void(const std::string&)> debug,
  std::function<void(const std::string&,const std::string&,const Json&)>
    localDiagnostics);
void bindSettingsConsumers(Store& store,LoadServices& loader,
  persistence::MdmPollServices& poll);
}
