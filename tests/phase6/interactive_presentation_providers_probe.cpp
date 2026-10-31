#include "interactive/presentation_providers/DefaultBindings.hpp"
#include "interactive/presentation_providers/CardPalettes.hpp"
#include "interactive/presentation_providers/Chrome.hpp"
#include "interactive/presentation_providers/SystemTheme.hpp"
#include "interactive/presentation_providers/ThemeData.hpp"
#include "interactive/presentation_providers/ThemeState.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <ftxui/screen/screen.hpp>

using namespace batbox;
using namespace batbox::interactive::presentation_providers;
namespace {
Json bindingTable(const std::string& platform,bool vt,
    const std::vector<std::string>& features){
  DefaultBindingOptions options{platform,vt,
    [features](const auto& name){
      return std::find(features.begin(),features.end(),name)!=features.end();
    }};
  Json result=Json::array();
  for(const auto& block:defaultBindingBlocks(options)){
    Json bindings=Json::array();
    for(const auto& [key,action]:block.bindings)
      bindings.push_back(Json::array({key,action.value_or("")}));
    result.push_back({{"context",block.context},{"bindings",bindings}});
  }
  return result;
}
}
int main(){
  Json output=Json::object();
  output["bindings"]=Json::array({
    bindingTable("linux",true,{}),
    bindingTable("linux",true,{"KAIROS","QUICK_SEARCH","TERMINAL_PANEL",
      "MESSAGE_ACTIONS","VOICE_MODE"}),
    bindingTable("windows",false,{"MESSAGE_ACTIONS"}),
    bindingTable("windows",true,{"QUICK_SEARCH"})});
  output["themes"]=Json::object();
  for(const auto& name:themeNames()){
    Json theme=Json::object();
    for(const auto& [key,value]:getTheme(name))theme[key]=value;
    output["themes"][name]=theme;
  }
  output["unknownTheme"]=Json::object();
  for(const auto& [key,value]:getTheme("missing"))
    output["unknownTheme"][key]=value;
  output["shortcut"]=Json::array();
  for(const auto& key:{"ctrl+shift+k","alt+←","cmd+return"," ",
      "ctrl+k ctrl+s","meta+esc"}){
    auto parsed=parseChord(key);
    Json ks=Json::array();
    for(const auto& item:parsed)
      ks.push_back({{"key",item.key},{"ctrl",item.ctrl},{"alt",item.alt},
        {"shift",item.shift},{"meta",item.meta},{"super",item.super}});
    output["shortcut"].push_back({{"parsed",ks},{"canonical",chordToString(parsed)},
      {"mac",chordToDisplayString(parsed,"macos")},
      {"linux",chordToDisplayString(parsed,"linux")}});
  }
  auto bindings=parseBindings({{"Global",{{"ctrl+o","app:toggleTranscript"},
    {"meta+o","app:toggleTranscript"}}}});
  output["lookup"]=getBindingDisplayText("app:toggleTranscript","Global",
    bindings).value_or("undefined");
  output["systemTheme"]=Json::array();
  for(const auto& value:{"rgb:ffff/ffff/ffff","rgb:0000/0000/0000",
      "#ffffff","#000000","nonsense"})
    output["systemTheme"].push_back(
      themeFromOscColor(value).value_or("undefined"));
  std::string saved;
  ThemeState theme({[]{return std::string("auto");},
    [&](const auto& value){saved=value;},[]{return std::string("light");},
    []{}});
  output["themeState"]=Json::array({theme.currentTheme()});
  theme.setPreview("dark");output["themeState"].push_back(theme.currentTheme());
  theme.cancelPreview();output["themeState"].push_back(theme.currentTheme());
  theme.setSetting("dark");output["themeState"].push_back(theme.currentTheme());
  output["saved"]=saved;
  auto shortcutProvider=makeShortcutProvider({"linux",true,
    [](const auto&){return false;}},[]{return std::vector<Binding>{};});
  auto chrome=makeChromeProviders({theme,shortcutProvider,
    []{return std::string("local model");},
    []{return std::string("/work");},
    []{return std::string("plan");},
    []{return std::string("ready");}});
  interactive::PresentationState state;
  state.loading=true;
  state.modal=interactive::UiRequest{interactive::UiRequest::Kind::Select,
    "modal","Choose action","Select one",{"First","Second"},1};
  interactive::transcript_navigation::StickyPoetry sticky;
  std::optional<interactive::message_actions::Cursor> cursor;
  interactive::conversation_shell::ViewState view{state,sticky,cursor,{},"",
    0,0,2,80,24,false,false,true};
  auto frame=ftxui::vbox({chrome.renderHeader(view),chrome.renderFooter(view),
    chrome.renderModal(view),chrome.renderPill(view),chrome.renderLogo(),
    chrome.renderDivider({interactive::message_list_view::DividerKind::Unseen,
      0,80})});
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
    ftxui::Dimension::Fixed(16));
  ftxui::Render(screen,frame);
  const auto rendered=screen.ToString();
  for(const auto& label:{"Batbox","local model","ctrl+o transcript",
      "Choose action","Second","Go to the action","0 new messages"})
    if(rendered.find(label)==std::string::npos)
      throw std::runtime_error(std::string("Missing chrome: ")+label);
  const auto paint=[&](const ftxui::Element& element,int columns,int rows){
    auto surface=ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
      ftxui::Dimension::Fixed(rows));
    ftxui::Render(surface,element);return surface.ToString();
  };
  for(const auto [columns,rows]:{std::pair{80,24},std::pair{24,12}}){
    view.columns=columns;view.rows=rows;
    const auto banner=paint(chrome.renderWelcome(view),columns,rows);
    if(banner.find("Batbox")==std::string::npos||
        banner.find("(o.o)")==std::string::npos||
        banner.find("(o o)")==std::string::npos)
      throw std::runtime_error("Startup companions missing at supported terminal size");
  }
  view.columns=80;view.rows=24;view.bodyRows=21;view.bodyScreenRow=1;
  std::string entrance,rest;
  int stageHeight=-1;
  for(unsigned index=0;index<=interactive::conversation_shell::welcomeAnimationFrames;++index){
    view.welcomeFrame=index;
    auto welcome=chrome.renderWelcome(view);welcome->ComputeRequirement();
    if(stageHeight<0)stageHeight=welcome->requirement().min_y;
    if(stageHeight!=welcome->requirement().min_y)
      throw std::runtime_error("Welcome animation changes scroll geometry");
    const auto painted=paint(welcome,80,21);
    if(painted.find("(o.o)")==std::string::npos||painted.find("(o o)")==std::string::npos)
      throw std::runtime_error("A companion disappears during the entrance");
    if(index==0)entrance=painted;
    if(index==interactive::conversation_shell::welcomeAnimationFrames)rest=painted;
  }
  if(entrance==rest)throw std::runtime_error("Welcome entrance is static");
  auto positioned=ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
    ftxui::Dimension::Fixed(21));
  ftxui::Render(positioned,chrome.renderWelcome(view));
  int firstRow=21;
  for(int y=0;y<21&&firstRow==21;++y)
    for(int x=0;x<80;++x)
      if(!positioned.CellAt(x,y).character.empty()&&
          positioned.CellAt(x,y).character!=" "){firstRow=y;break;}
  if(firstRow+view.bodyScreenRow!=view.rows/3)
    throw std::runtime_error("Welcome does not rest at the upper third");
  view.welcomeFrame+=100;
  if(paint(chrome.renderWelcome(view),80,21)!=rest)
    throw std::runtime_error("Welcome does not settle at its final frame");
  for(const auto [columns,rows]:{std::pair{24,6},std::pair{20,4},
      std::pair{17,4},std::pair{11,4},std::pair{9,3},std::pair{8,3}}){
    view.columns=columns;view.rows=rows;view.bodyRows=rows;view.bodyScreenRow=0;
    const auto tiny=paint(chrome.renderWelcome(view),columns,rows);
    if(columns>=12){
      if(tiny.find("/\\^/\\")==std::string::npos||tiny.find("(o o)~")==std::string::npos)
        throw std::runtime_error("Short terminal lost a companion");
    }else if(columns>=9){
      if(tiny.find("/\\^/\\")==std::string::npos||tiny.find("(oo)")==std::string::npos)
        throw std::runtime_error("Narrow terminal lost a companion");
    }else if(tiny.find("^v^")==std::string::npos||tiny.find("(oo)~")==std::string::npos)
      throw std::runtime_error("Tiny terminal lost companion initials");
  }
  for(const auto columns:{24,17,9}){
    view.columns=columns;view.rows=4;view.bodyRows=1;view.bodyScreenRow=1;
    auto viewport=interactive::viewport::Controller::create([]{});
    const auto single=paint(viewport->wrap(chrome.renderWelcome(view),1),columns,1);
    viewport->committed();
    if(single.find("/\\^/\\")==std::string::npos||
        single.find(columns>=12?"(o o)~":"(oo)")==std::string::npos)
      throw std::runtime_error("Single-row viewport scrolled past the companions");
  }
  view.columns=80;view.rows=24;view.remembering=true;
  const auto bright=paint(chrome.renderHeader(view),80,1);
  view.activityFrame=8;
  const auto dim=paint(chrome.renderHeader(view),80,1);
  if(bright.find("remembering")==std::string::npos||bright==dim)
    throw std::runtime_error("Remembering activity does not pulse");
  theme.setSetting("monochrome");
  view.bodyRows=21;view.bodyScreenRow=1;
  if(paint(chrome.renderWelcome(view),80,21).find("[38;")!=std::string::npos)
    throw std::runtime_error("Monochrome welcome gained an explicit color");
  const auto plain=paint(chrome.renderHeader(view),80,1);
  if(plain.find("remembering")==std::string::npos||
      plain.find("[38;")!=std::string::npos)
    throw std::runtime_error("Monochrome activity gained an explicit color");
  view.remembering=false;state.loading=false;
  if(paint(chrome.renderHeader(view),80,1).find("remembering")!=std::string::npos)
    throw std::runtime_error("Remembering activity remained after recovery");
  const auto palette=assistantPalette(theme);
  (void)palette;
  output["chromeRendered"]=true;
  std::cout<<output.dump()<<'\n';
}
