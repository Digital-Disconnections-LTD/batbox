#include "interactive/cards_gadget_children/GadgetSuccessCard.hpp"
#include <fstream>
#include <iostream>

using namespace batbox::interactive;
using namespace batbox::interactive::cards_gadget_children;
using batbox::Json;

int main(int argc,char** argv){
  if(argc!=3||std::string(argv[1])!="--cases")return 2;
  std::ifstream stream(argv[2]);
  Json output=Json::array();
  for(const auto& item:Json::parse(stream)){
    std::vector<std::string> trace;
    Json parsed=Json();
    Json input=Json();
    Json brief=Json();
    batbox::gadgets::Gadget gadget;
    gadget.name="TestTool";
    if(item.value("hasSchema",false))gadget.outputSchema=Json::object();
    if(item.value("hasRenderer",false))
      gadget.renderGadgetResultMessage=[](const Json&,const Json&,const Json&){
        return Json("unused-adapter-marker");
      };
    batbox::gadgets::Gadgets gadgets{&gadget};
    MessageLookups lookups;
    auto owner=std::make_shared<Json>(Json{{"type","assistant"},
      {"message",Json{{"content",Json::array({
        Json{{"type","tool_use"},{"id","tool"},
             {"name","TestTool"},
             {"input",Json{{"command","pwd"}}}}
      })}}}});
    lookups.gadgetUseByGadgetUseID["tool"]={owner,0};
    SuccessServices services;
    services.briefFeature=item.value("briefFeature",false);
    services.classifierApprovalFeature=
      item.value("classifierApprovalFeature",false);
    services.classifierDenialFeature=
      item.value("classifierDenialFeature",false);
    services.readTheme=[&]{trace.push_back("theme");
      return Json{{"name","theme"}};};
    services.readBriefOnly=[&]{trace.push_back("brief");
      return item.value("briefOnly",false);};
    services.getClassifierApproval=[&](const std::string&){
      trace.push_back("getRule");
      return item.contains("classifierRule")?
        std::optional<std::string>{item.at("classifierRule").get<std::string>()}:
        std::nullopt;
    };
    services.getYoloClassifierApproval=[&](const std::string&){
      trace.push_back("getYolo");
      return item.contains("yoloReason")?
        std::optional<std::string>{item.at("yoloReason").get<std::string>()}:
        std::nullopt;
    };
    services.deleteClassifierApproval=[&](const std::string&){
      trace.push_back("delete");};
    services.safeParseOutput=[&](const batbox::gadgets::Gadget&,const Json&){
      trace.push_back("safeParse");
      return item.value("schemaValid",false)?
        std::optional<Json>{Json{{"validated",true}}}:
        std::nullopt;
    };
    services.renderGadgetResult=[&](const batbox::gadgets::Gadget&,
        const Json& value,const Json&,const SuccessRenderContext& context){
      trace.push_back("render");
      parsed=value;
      input=context.input?*context.input:Json();
      brief=context.briefOnly;
      return item.value("renderNull",false)?Json():Json("RESULT");
    };
    services.userFacingName=[&](const batbox::gadgets::Gadget&){
      trace.push_back("name");
      return item.at("displayName").get<std::string>();};
    Json message=Json::object();
    if(item.value("hasResult",false))
      message["toolUseResult"]=item.contains("toolResult")?
        item.at("toolResult"):Json{{"ok",true}};
    GadgetSuccessState state;
    const auto card=state.render(message,"tool",Json::array(),lookups,
      item.value("hasTool",false)?&gadget:nullptr,gadgets,{},false,false,
      services);
    state.committed("tool",services);
    output.push_back({{"name",item.at("name")},
      {"visible",card.visible},
      {"assistantText",card.visible&&card.rendersAsAssistantText},
      {"approval",card.visible&&card.classifierRule.has_value()},
      {"autoApproval",card.visible&&card.yoloReason.has_value()},
      {"hook",card.visible},
      {"parsed",parsed},{"input",input},{"brief",brief},
      {"trace",trace}});
  }
  std::cout<<output.dump()<<'\n';
}
