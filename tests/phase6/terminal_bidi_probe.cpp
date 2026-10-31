#include "terminal_output/Bidi.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>

using namespace batbox;
namespace output=batbox::terminal_output;
namespace {
Json run(const std::string& kind,const output::BidiEnvironment& env,
    const std::vector<std::string>& values){
  int factoryCalls=0;
  const auto factory=output::indigenousBidiFactory();
  output::BidiReorder bidi([env]{return env;},
    [&]()->output::LevelsProvider{factoryCalls++;return factory();});
  auto input=std::make_shared<output::ClusteredChars>();
  for(const auto& value:values)
    input->push_back(std::make_shared<output::ClusteredChar>(
      output::ClusteredChar{value,1,0,{}}));
  const auto first=bidi.reorder(input);
  const auto second=bidi.reorder(input);
  Json result=Json::array();bool sameObjects=true;
  for(const auto& character:*first){
    result.push_back(character->value);
    if(std::find(input->begin(),input->end(),character)==input->end())
      sameObjects=false;
  }
  return Json{{"kind",kind},{"value",result},
    {"sameArray",first==input},{"sameObjects",sameObjects},
    {"secondSameArray",second==input},{"factoryCalls",factoryCalls}};
}
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])=="--matrix"){
    Json cases;std::cin>>cases;
    output::BidiReorder bidi([]{return output::BidiEnvironment{"win32",{},{}};},
      output::indigenousBidiFactory());
    Json results=Json::array();
    for(const auto& values:cases){
      auto input=std::make_shared<output::ClusteredChars>();
      for(const auto& value:values)
        input->push_back(std::make_shared<output::ClusteredChar>(
          output::ClusteredChar{value.get<std::string>(),1,0,{}}));
      Json row=Json::array();
      const auto reordered=bidi.reorder(input);
      for(const auto& character:*reordered)
        row.push_back(character->value);
      results.push_back(row);
    }
    std::cout<<results.dump()<<'\n';
    return 0;
  }
  Json rows=Json::array();
  rows.push_back(run("linuxNoBidi",{"linux",{},{}},{"a","ב","ג","c"}));
  rows.push_back(run("winAscii",{"win32",{},{}},{"a","b","c"}));
  rows.push_back(run("winHebrew",{"win32",{},{}},{"a","ב","ג","c"}));
  rows.push_back(run("wtEmpty",{"linux",std::string(""),{}},
    {"A","س","ل","ا","م","1"}));
  rows.push_back(run("vscode",{"linux",{},std::string("vscode")},
    {"א","ב","c","2"}));
  rows.push_back(run("cluster",{"win32",{},{}},
    {"x","👩‍💻","שׁ","ל","ו","ם","!"}));
  std::cout<<rows.dump()<<'\n';
}
