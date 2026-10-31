#include "interactive/cards_collapsed/VerboseGadgetUse.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_collapsed {
namespace {
void require(const auto& fn,const char* name){
  if(!fn)throw std::logic_error(std::string(name)+" import required");
}
std::string field(const Json& value,const char* key){
  const auto it=value.find(key);
  return it!=value.end()&&it->is_string()?it->get<std::string>():"";
}
}
VerboseGadgetUseCard classifyVerboseGadgetUse(const Json& content,
    const gadgets::Gadgets& availableGadgets,const MessageLookups& lookups,
    const GadgetIDs& inProgressGadgetUseIDs,bool shouldAnimate,
    const Json& theme,const VerboseGadgetUseServices& services){
  require(services.readSelectedBackground,"selected message background");
  VerboseGadgetUseCard card;
  card.selectedBackground=services.readSelectedBackground();
  const auto id=field(content,"id");
  const auto name=field(content,"name");
  auto gadget=gadgets::findGadgetByName(availableGadgets,name);
  if(!gadget){
    require(services.primitiveGadgets,"primitive Gadget Roster");
    const auto primitive=services.primitiveGadgets();
    gadget=gadgets::findGadgetByName(primitive,name);
  }
  if(!gadget)return card;
  card.visible=true;
  card.resolved=lookups.resolvedGadgetUseIDs.contains(id);
  card.error=lookups.erroredGadgetUseIDs.contains(id);
  card.inProgress=inProgressGadgetUseIDs.contains(id);
  card.shouldAnimate=shouldAnimate;
  std::optional<Json> rawResult;
  const auto result=lookups.gadgetResultByGadgetUseID.find(id);
  if(result!=lookups.gadgetResultByGadgetUseID.end()&&result->second&&
     field(*result->second,"type")=="user"){
    const auto value=result->second->find("toolUseResult");
    if(value!=result->second->end())rawResult=*value;
  }
  std::optional<Json> gadgetResult;
  if(!gadget->outputSchema.is_null()){
    require(services.safeParseOutput,"Gadget output schema");
    gadgetResult=services.safeParseOutput(*gadget,rawResult);
  }
  require(services.safeParseInput,"Gadget input schema");
  const auto parsed=services.safeParseInput(*gadget,content.value("input",Json()));
  require(services.userFacingName,"Gadget display name");
  card.name=services.userFacingName(*gadget,parsed);
  if(parsed&&value::truthy(*parsed)){
    require(services.renderUse,"verbose Gadget use renderer");
    card.useMessage=services.renderUse(*gadget,*parsed,theme);
    if(gadget->renderGadgetUseTag){
      require(services.renderTag,"verbose Gadget tag renderer");
      card.tag=services.renderTag(*gadget,*parsed);
    }
  }
  if(card.resolved&&!card.error&&gadgetResult){
    card.hasResultMessage=true;
    if(gadget->renderGadgetResultMessage){
      require(services.renderResult,"verbose Gadget result renderer");
      card.resultMessage=services.renderResult(*gadget,*gadgetResult,
                                               availableGadgets,theme);
    }
  }
  return card;
}
}
