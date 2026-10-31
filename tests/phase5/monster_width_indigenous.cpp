#include "monster/WidthBinding.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace batbox;
 auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());
 auto width=std::make_shared<text_width::StringWidth>(text_width::indigenousServices(intl));
 monster::GeometryServices s;
 s.feature=[](const std::string& key){return key=="MONSTER";};
 s.getMonster=[]()->std::optional<Json>{return Json{{"name","漢字漢字漢字漢字漢字"}};};
 s.getGlobalConfig=[](){return Json::object();};s.isFullscreenActive=[](){return false;};
 monster::bindGeometryWidth(s,width);
 assert(width->stringWidth("漢字漢字漢字漢字漢字")==20);
 assert(monster::monsterReservedColumns(120,false,s)==24);
 assert(monster::monsterReservedColumns(120,true,s)==60);
 assert(monster::monsterReservedColumns(80,true,s)==0);
 std::cout<<"Native pinned-SDK width bound to Unicode monster terminal geometry passes\n";
}
