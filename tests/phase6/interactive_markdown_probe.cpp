#include "interactive/markdown/Renderer.hpp"
#include "interactive/markdown/AnsiProjection.hpp"
#include "runtime/JsonValue.hpp"
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/node.hpp>
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive::markdown;
int main(){
  Json cases;std::cin>>cases;
  auto services=indigenousFormatServices(false,false,"",[](const std::string& text){
    return static_cast<int>(text.size());});
  const WrapAnsi wrap=[](const std::string& value,int,bool){return value;};
  Renderer renderer(services,wrap,70);
  Json result={{"format",Json::array()},{"stream",Json::array()}};
  for(const auto& item:cases["format"]){
    const auto input=item["text"].get<std::string>();
    const auto stripped=stripPoetryXMLTags(input);
    const auto formatted=value::trim(formatTokens(lex(stripped),services));
    const auto tokens=lex(stripped);
    std::optional<std::string> table;
    for(const auto& token:tokens)
      if(token.kind==Kind::Table){table=renderTable(token,70,services,wrap);break;}
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(70),
      ftxui::Dimension::Fixed(8));
    ftxui::Render(screen,renderer.render(input));
    const auto drawing=screen.ToString();
    assert(!drawing.empty());
    if(item["name"]=="heading")assert(drawing.find("Heading")!=std::string::npos);
    if(item["name"]=="code")assert(drawing.find("const x = 1;")!=std::string::npos);
    if(item["name"]=="table")assert(drawing.find("┌──────")!=std::string::npos);
    result["format"].push_back(Json{{"name",item["name"]},
      {"formatted",formatted},{"table",table?Json(*table):Json()}});
  }
  StreamingState state;
  for(const auto& text:cases["stream"]){
    const auto input=text.get<std::string>();
    (void)renderer.renderStreaming(state,input);
    const auto stripped=stripPoetryXMLTags(input);
    result["stream"].push_back(Json{{"stable",state.stablePrefix},
      {"suffix",stripped.substr(state.stablePrefix.size())}});
  }
  auto colorScreen=ftxui::Screen::Create(ftxui::Dimension::Fixed(30),
    ftxui::Dimension::Fixed(2));
  ftxui::Render(colorScreen,projectAnsi("\x1b[1mBold\x1b[22m and plain"));
  assert(colorScreen.ToString().find("\x1b[1mBold\x1b[22m and plain")!=
    std::string::npos);
  std::cout<<result.dump()<<'\n';
}
