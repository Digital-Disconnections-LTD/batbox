#include "settings/schema/Main.hpp"
#include "settings/schema/Descriptor.hpp"
#include "settings/schema/Runtime.hpp"

namespace batbox::settings::schema {
namespace detail {
Json fieldsForConfig(const Json& descriptor,const MainConfig& config){
  Json fields=descriptor.at("base").at("fields");
  const Json& variants=descriptor.at("variants");
  const std::pair<const char*,bool> flags[]={{"XAA",config.xaa},
    {"TRANSCRIPT_CLASSIFIER",config.transcriptClassifier},
    {"LODESTONE",config.lodestone},{"PROACTIVE",config.proactive},
    {"KAIROS",config.kairos},{"VOICE_MODE",config.voiceMode},
    {"KAIROS_BRIEF",config.kairosBrief},{"INTERNAL",config.internal}};
  for(const auto& [name,enabled]:flags){
    if(!enabled||!variants.contains(name))continue;
    for(const auto& addition:variants[name]){
      bool already=false;
      for(const auto& field:fields)
        if(field[0]==addition.at("key")){already=true;break;}
      if(already)continue;
      Json next=Json::array();
      bool inserted=false;
      for(const auto& field:fields){
        next.push_back(field);
        if(!inserted&&field[0]==addition.at("after")){
          next.push_back(Json::array({addition.at("key"),addition.at("node")}));
          inserted=true;
        }
      }
      if(!inserted)next.push_back(Json::array(
        {addition.at("key"),addition.at("node")}));
      fields=std::move(next);
    }
  }
  if(config.internal)for(auto& field:fields)
    if(field[0]=="effortLevel")
      field[1]=descriptor.at("overrides").at("internalEffortLevel");
  if(config.internal&&config.transcriptClassifier)for(auto& field:fields)
    if(field[0]=="autoMode")
      field[1]=descriptor.at("overrides").at("classifierInternalAutoMode");
  return fields;
}
}
SchemaResult settings(const Json& input,const MainConfig& config,
  const MainImports& imports){
  Json node=settingsDescriptor().at("base");
  node["fields"]=detail::fieldsForConfig(settingsDescriptor(),config);
  auto parsed=detail::parseNode(node,input,true,Json::array(),config,imports);
  return {parsed.valid,parsed.valid?parsed.value:Json(nullptr),parsed.issues};
}
}
