#include "terminal_color/Colorize.hpp"
#include <iostream>

using namespace batbox;
namespace tc=batbox::terminal_color;
int main(){
  Json rows=Json::array();
  for(const auto& [kind,level,term,tmux,truecolor]:
      std::vector<std::tuple<std::string,int,
        std::optional<std::string>,std::optional<std::string>,bool>>{
      {"vscodeBoost",2,"vscode",{},false},
      {"tmuxClamp",3,{},"1",false},
      {"vscodeTmux",2,"vscode","1",false},
      {"forceNone",0,"vscode",{},false},
      {"tmuxTruecolor",3,{},"1",true}}){
    const auto state=tc::initializeChalkState(level,term,tmux,truecolor);
    rows.push_back(Json{{"kind",kind},{"level",state.level},
      {"boosted",state.boostedForXtermJs},
      {"clamped",state.clampedForTmux}});
  }
  for(const auto& [kind,level,color,type,value]:
      std::vector<std::tuple<std::string,int,std::string,
        tc::ColorType,std::string>>{
      {"namedFg",3,"ansi:red",tc::ColorType::Foreground,"hello"},
      {"namedBg",3,"ansi:blueBright",tc::ColorType::Background,"hello"},
      {"namedNone",0,"ansi:red",tc::ColorType::Foreground,"hello"},
      {"unknown",3,"ansi:chartreuse",tc::ColorType::Foreground,"hello"},
      {"hexTrue",3,"#D77757",tc::ColorType::Foreground,"color"},
      {"hex256",2,"#D77757",tc::ColorType::Background,"color"},
      {"hex16",1,"#D77757",tc::ColorType::Foreground,"color"},
      {"hexBad",3,"#nothex",tc::ColorType::Foreground,"color"},
      {"rgbTrue",3,"rgb(215,119,87)",tc::ColorType::Foreground,"rgb"},
      {"rgb256",2,"rgb(215,119,87)",tc::ColorType::Background,"rgb"},
      {"rgb16",1,"rgb(215,119,87)",tc::ColorType::Foreground,"rgb"},
      {"gray256",2,"rgb(240,240,240)",tc::ColorType::Foreground,"gray"},
      {"ansi256",1,"ansi256(255)",tc::ColorType::Background,"ansi"},
      {"ansiBad",3,"ansi256( 7  )",tc::ColorType::Foreground,"ansi"},
      {"rgbSpace",3,"rgb(\xC2\xA0" "215,\t119, 87\xE2\x80\xA9)",
        tc::ColorType::Foreground,"rgb"},
      {"newlines",3,"ansi:red",tc::ColorType::Foreground,"a\r\nb\nc"},
      {"nested",3,"ansi:red",tc::ColorType::Foreground,"a\x1b[39mb"}}){
    rows.push_back(Json{{"kind",kind},{"output",tc::colorize(value,color,
      type,tc::ChalkState{static_cast<double>(level)})}});
  }
  for(const auto& [kind,level,styles,value]:
      std::vector<std::tuple<std::string,int,Json,std::string>>{
      {"boldDim",3,Json{{"bold",true},{"dim",true}},"text"},
      {"allModifiers",3,Json{{"inverse",true},{"strikethrough",true},
        {"underline",true},{"italic",true},{"bold",true},{"dim",true}},
        "text"},
      {"colors",3,Json{{"color","ansi:green"},
        {"backgroundColor","#112233"}},"a\nb"},
      {"stylesNone",0,Json{{"bold",true},{"color","ansi:red"}},"text"},
      {"stylesEmpty",3,Json{{"bold",true}},""}}){
    rows.push_back(Json{{"kind",kind},
      {"output",tc::applyTextStyles(value,
        std::make_shared<Json>(styles),tc::ChalkState{static_cast<double>(level)})}});
  }
  rows.push_back(Json{{"kind","applyColorMissing"},
    {"output",tc::applyColor("plain",{},tc::ChalkState{3})}});
  std::cout<<rows.dump()<<'\n';
}
