#include "interactive/cards_gadget_children/GadgetRejectCard.hpp"
#include <fstream>
#include <iostream>

using namespace batbox::interactive::cards_gadget_children;
using batbox::Json;

int main(int argc,char** argv){
  if(argc!=3||std::string(argv[1])!="--cases")return 2;
  std::ifstream stream(argv[2]);
  Json output=Json::array();
  for(const auto& item:Json::parse(stream)){
    std::vector<std::string> trace;
    Json options=Json();
    batbox::gadgets::Gadget gadget;
    if(item.value("hasRenderer",false))
      gadget.renderGadgetUseRejectedMessage=[](const Json&,const Json&){
        return Json("unused-adapter-marker");};
    batbox::gadgets::Gadgets gadgets{&gadget};
    RejectServices services;
    services.readColumns=[&]{trace.push_back("columns");return 80;};
    services.readTheme=[&]{trace.push_back("theme");
      return Json{{"name","theme"}};};
    services.safeParseInput=[&](const batbox::gadgets::Gadget&,const Json&){
      trace.push_back("safeParse");
      return item.value("schemaValid",false)?
        std::optional<Json>{Json{{"validated",true}}}:std::nullopt;};
    services.renderRejected=[&](const batbox::gadgets::Gadget&,const Json& value,
                                 const RejectRenderContext& context){
      trace.push_back("render");
      options={{"value",value},{"columns",context.columns},
        {"style",context.style?Json(*context.style):Json()},
        {"transcript",context.transcriptMode}};
      return item.value("renderNull",false)?Json():Json("RENDERED");};
    const auto card=classifyGadgetReject(Json{{"command","pwd"}},
      Json::array(),item.value("hasTool",false)?&gadget:nullptr,gadgets,
      item.contains("style")?
        std::optional<std::string>{item.at("style").get<std::string>()}:
        std::nullopt,item.value("verbose",false),
      item.value("transcript",false),services);
    output.push_back({{"name",item.at("name")},{"fallback",card.fallback},
      {"rendered",card.fallback?Json():card.renderedMessage},
      {"options",options},{"trace",trace}});
  }
  std::cout<<output.dump()<<'\n';
}
