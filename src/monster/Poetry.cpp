#include "monster/Poetry.hpp"
namespace batbox::monster {
std::string monsterIntroText(const std::string& name,const std::string& species){
  return "# Monster\n\nA small "+species+" named "+name+" sits beside the user's input box and occasionally comments in a speech bubble. You're not "+name+" — it's a separate watcher.\n\nWhen the user addresses "+name+" directly (by name), its bubble will answer. Your job in that moment is to stay out of the way: respond in ONE line or less, or just answer any part of the message meant for you. Don't explain that you're not "+name+" — they know. Don't narrate what "+name+" might say — the bubble handles that.";
}
Json getMonsterIntroAttachment(const std::optional<Json>& messages,const IntroServices& s){
  if(!s.feature("MONSTER"))return Json::array();
  const auto monster=s.getMonster();
  if(!monster||value::truthy(value::property(s.getGlobalConfig(),"monsterMuted")))return Json::array();
  if(messages)for(const auto& msg:*messages){
    if(value::property(msg,"type")!="attachment")continue;
    const auto& attachment=value::property(msg,"attachment");
    if(value::property(attachment,"type")!="monster_intro")continue;
    if(value::property(attachment,"name")==monster->at("name"))return Json::array();
  }
  return Json::array({Json{{"type","monster_intro"},{"name",monster->at("name")},{"species",monster->at("species")}}});
}
}
