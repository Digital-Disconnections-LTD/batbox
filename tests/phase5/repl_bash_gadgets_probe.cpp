#include "interactive/lifecycle/BashGadgets.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  const std::vector<std::string> commands={"","   ","git status",
    "sudo git status","FOO=bar vercel","FOO=bar sudo aws s3",
    "9FOO=x python","_X=1\xc2\xa0" "sudo\xc2\xa0" "npm test",
    "\xc3\x89=1 node","sudo","sudo sudo","VAR= x",
    "\xe2\x80\x89" "foo\xe2\x80\x89" "bar","X=1\n\tgit"};
  Json names=Json::array();
  for(const auto& command:commands){
    const auto name=extractCliName(command);
    names.push_back(name?Json(*name):Json());
  }
  auto messages=std::make_shared<TurnMessages>();
  messages->push_back(std::make_shared<Json>(Json{{"type","assistant"},
    {"message",{{"content",Json::array({
      {{"type","tool_use"},{"name","Bash"},{"input",{{"command","sudo git status"}}}},
      {{"type","tool_use"},{"name","Bash"},{"input",{{"command","git add"}}}},
      {{"type","tool_use"},{"name","Bash"},{"input",{{"command","FOO=bar node a"}}}},
      {{"type","tool_use"},{"name","Other"},{"input",{{"command","ignored"}}}}
    })}}}}));
  messages->push_back(std::make_shared<Json>(Json{{"type","user"},
    {"message",{{"content",Json::array({
      {{"type","tool_use"},{"name","Bash"},{"input",{{"command","user"}}}}
    })}}}}));
  messages->push_back(std::make_shared<Json>(Json{{"type","assistant"},
    {"message",{{"content",Json::array({
      {{"type","tool_use"},{"name","Bash"},{"input",{{"command","sudo aws"}}}},
      {{"type","tool_use"},{"name","Bash"},{"input",{{"command",7}}}}
    })}}}}));
  Json gadgets=Json::array();
  for(const auto& value:extractBashGadgetsFromMessages(messages,0))gadgets.push_back(value);
  std::cout<<Json{{"names",names},{"tools",gadgets}}.dump()<<'\n';
}
