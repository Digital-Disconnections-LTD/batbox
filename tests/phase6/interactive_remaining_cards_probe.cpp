#include "interactive/cards_remaining/AttachmentCard.hpp"
#include "interactive/cards_remaining/MessageLeaves.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json cases;std::cin>>cases;
  Json output=Json::array();
  interactive::cards_remaining::AttachmentServices services;
  services.readSelectedBackground=[](){return Json();};
  services.batSwarmsEnabled=[](){return false;};
  services.skillSearchEnabled=[](){return false;};
  services.displayPath=[](const std::string& value){return "display:"+value;};
  for(const auto& item:cases){
    const auto kind=item.value("kind",std::string());
    if(kind=="image"){
      interactive::cards_remaining::ImageServices image;
      image.getStoredImagePath=[](int id)->std::optional<std::string>{
        if(id==2)return "/tmp/p.png";
        return {};
      };
      image.supportsHyperlinks=[](){return true;};
      image.pathToFileURL=[](const std::string& path){return "file://"+path;};
      const auto card=interactive::cards_remaining::classifyUserImage(
        item["imageID"].get<int>(),false,image);
      output.push_back(Json{{"name",item["name"]},{"visible",true},
        {"text",card.label}});
      continue;
    }
    if(kind=="compact"){
      interactive::cards_remaining::CompactServices compact;
      compact.getUserMessageText=[](const Json& message){
        return message.value("text",std::string());};
      compact.shortcutHint=[](const std::string& description){
        return "(ctrl+o to "+description+")";};
      const auto card=interactive::cards_remaining::classifyCompactSummary(
        item["message"],item["screen"],compact);
      std::string text="●"+card.title;
      if(card.metadata&&!card.transcriptMode){
        text+=card.body+card.context+card.shortcut;
      }else if(card.transcriptMode)text+=card.body;
      else text+=" "+card.shortcut;
      output.push_back(Json{{"name",item["name"]},{"visible",true},
        {"text",text}});
      continue;
    }
    if(kind=="boundary"){
      const auto value=interactive::cards_remaining::compactBoundaryText(
        []{return "Ctrl+O";});
      output.push_back(Json{{"name",item["name"]},{"visible",true},
        {"text",value}});
      continue;
    }
    if(kind=="advisor"){
      interactive::cards_remaining::AdvisorServices advisor;
      advisor.renderModelName=[](const std::string& model){return model;};
      advisor.expandHint=[](){return "(Ctrl+O to expand)";};
      const auto model=item.contains("advisorModel")?
        std::optional<std::string>(item["advisorModel"].get<std::string>()):
        std::optional<std::string>{};
      const auto card=interactive::cards_remaining::classifyAdvisor(
        item["block"],false,{}, {},false,false,model,advisor);
      std::string text=card.kind==interactive::cards_remaining::AdvisorCard::Kind::GadgetUse?
        "Advising"+(card.modelName.empty()?"":" using "+card.modelName)+
          (card.input.empty()?"":" · "+card.input):
        card.kind==interactive::cards_remaining::AdvisorCard::Kind::Result||
        card.kind==interactive::cards_remaining::AdvisorCard::Kind::Redacted?
          "✓ "+card.text:card.text;
      output.push_back(Json{{"name",item["name"]},
        {"visible",card.kind!=interactive::cards_remaining::AdvisorCard::Kind::Hidden},
        {"text",text}});
      continue;
    }
    if(kind=="header"){
      interactive::cards_remaining::PartnerHeaderServices header;
      header.readViewedPartner=[&item](){return std::optional<Json>(item["viewed"]);};
      header.exitShortcut=[](){return "esc return";};
      const auto card=interactive::cards_remaining::classifyPartnerHeader(header);
      output.push_back(Json{{"name",item["name"]},{"visible",card.visible},
        {"text","Viewing @"+card.name+" · "+card.exitHint+card.poetry}});
      continue;
    }
    const auto card=interactive::cards_remaining::classifyAttachment(
      item["attachment"],false,false,false,services);
    std::string text;
    if(card.kind==interactive::cards_remaining::AttachmentCard::Kind::Lines){
      for(const auto& line:card.lines)
        for(const auto& span:line.spans)text+=span.text;
    }else if(card.kind==interactive::cards_remaining::AttachmentCard::Kind::UserPoetry)
      text=card.poetryText;
    else if(card.kind==interactive::cards_remaining::AttachmentCard::Kind::Diagnostics)
      text="DIAGNOSTICS";
    output.push_back(Json{{"name",item["name"]},
      {"visible",card.kind!=interactive::cards_remaining::AttachmentCard::Kind::Hidden},
      {"text",text}});
  }
  std::cout<<output.dump()<<'\n';
}
