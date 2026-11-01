// Behavior of the visual layer below the golden frames: color-depth
// detection, theme resolution, the role table, Gadget view building,
// terminal-safe text, syntax highlighting, markdown blocks, /theme and /help.
#include "application_commands/ThemeCommand.hpp"
#include "commands_indigenous/UiCommands.hpp"
#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "interactive/markdown/AnsiProjection.hpp"
#include "interactive/markdown/Blocks.hpp"
#include "interactive/markdown/Highlight.hpp"
#include "interactive/markdown/Renderer.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "interactive/presentation_providers/ThemeData.hpp"
#include "terminal_ftxui/TextFit.hpp"
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace batbox;
namespace ip=batbox::interactive::presentation_providers;
namespace gv=batbox::interactive::gadget_views;
namespace md=batbox::interactive::markdown;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
std::string draw(const ftxui::Element& element,int width,int height){
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
    ftxui::Dimension::Fixed(height));
  ftxui::Render(screen,element);
  std::string text;
  for(int y=0;y<height;++y){
    for(int x=0;x<width;++x){
      const auto& glyph=screen.CellAt(x,y).character;
      text+=glyph.empty()?" ":glyph;
    }
    text+="\n";
  }
  return text;
}
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr failure;
  pending.then([&](const T* result,const std::exception_ptr& error){
    if(result)value=*result;failure=error;});
  for(int i=0;i<30&&!value&&!failure;++i)loop.runReady();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Command did not settle");
  return *value;
}

void colorDepth(){
  using D=ip::ColorDepth;
  const auto depth=[](std::string term,std::string colorTerm,std::string program,
      std::string noColor="",std::string wt=""){
    return ip::detectColorDepth({term,colorTerm,program,noColor,wt});
  };
  check(depth("xterm-256color","truecolor","","1")==D::None,"NO_COLOR wins");
  check(depth("dumb","","")==D::None,"dumb terminals get no color");
  check(depth("xterm-256color","truecolor","")==D::TrueColor,"COLORTERM truecolor");
  check(depth("xterm-256color","","iTerm.app")==D::TrueColor,
    "iTerm2 renders truecolor even when TERM says 256");
  for(const auto* program:{"WezTerm","ghostty","vscode"})
    check(depth("xterm-256color","",program)==D::TrueColor,
      std::string(program)+" renders truecolor");
  check(depth("xterm-256color","","Apple_Terminal")==D::Ansi256,
    "Terminal.app is limited to 256 colors");
  check(depth("xterm-256color","","")==D::Ansi256,"TERM 256 colors");
  check(depth("xterm-kitty","","")==D::TrueColor,"kitty");
  check(depth("vt100","","","","")==D::Ansi16,"unknown terminals use 16 colors");
  check(depth("vt100","","","","{guid}")==D::TrueColor,"Windows Terminal session");
}

void themes(){
  using D=ip::ColorDepth;
  check(ip::themeForSetting("auto",D::TrueColor,"light")=="batbox-light","auto light");
  check(ip::themeForSetting("auto",D::Ansi256,"dark")=="batbox-dark","auto dark");
  check(ip::themeForSetting("auto",D::Ansi16,"light")=="batbox-ansi","auto on 16 colors");
  check(ip::themeForSetting("batbox-light",D::None,"dark")=="monochrome",
    "no color means monochrome whatever the setting");
  check(ip::themeForSetting("halloween",D::TrueColor,"dark")=="batbox-dark",
    "the old halloween name keeps working");
  check(ip::themeForSetting("dark",D::Ansi16,"dark")=="dark-ansi",
    "classic themes fall back to their ANSI tables");
  const auto choices=ip::selectableThemes();
  check(!choices.empty()&&choices.front().name=="auto","the picker starts with auto");
  for(const auto& choice:choices)
    check(ip::isKnownTheme(choice.name),choice.name+" must resolve");
  check(!ip::isKnownTheme("neon"),"unknown themes are rejected");

  const auto& dark=ip::styleRoles("batbox-dark");
  check(dark.brand==ftxui::Color::RGB(185,148,247),"batbox-dark brand purple");
  check(dark.text==ftxui::Color::Default,"body text stays with the terminal");
  check(!(dark.codeBg==ftxui::Color::Default)&&!(dark.diffAddBg==ftxui::Color::Default),
    "truecolor themes tint code and diff rows");
  const auto& light=ip::styleRoles("batbox-light");
  check(light.light&&!(light.brand==dark.brand),"light theme has its own ink");
  const auto& ansi=ip::styleRoles("batbox-ansi");
  check(ansi.basic&&ansi.codeBg==ftxui::Color::Default&&ansi.selectionBg==ftxui::Color::Default,
    "16-color themes never paint panel tints");
  const auto& mono=ip::styleRoles("monochrome");
  check(mono.monochrome&&mono.brand==ftxui::Color::Default&&
    mono.statusErr==ftxui::Color::Default,"monochrome paints no color");
  const auto& classic=ip::styleRoles("dark");
  check(classic.brand==ip::themeColor("dark","brand"),"classic themes derive roles");
  check(ip::sgr(ftxui::Color::Default).empty(),"default color emits no SGR");
  check(ip::sgr(ftxui::Color::RGB(1,2,3))=="\x1b[38;2;1;2;3m","truecolor SGR");
  check(ip::sgr(ftxui::Color::RGB(1,2,3),true)=="\x1b[48;2;1;2;3m","background SGR");

  ip::applyColorDepth(D::Ansi16);
  check(ip::resolveThemeName("batbox-dark","dark")=="batbox-ansi",
    "an explicit truecolor theme adapts to a 16-color terminal");
  ip::applyColorDepth(D::TrueColor);
  check(ip::resolveThemeName("auto","light")=="batbox-light","auto resolves through depth");
}

void viewModel(){
  check(gv::sanitize("\x1b[31mred\x1b[0m plain")=="red plain","SGR stripped");
  check(gv::sanitize("\x1b]8;;https://x\x07link\x1b]8;;\x07")=="link","OSC 8 stripped");
  check(gv::sanitize("\x1b]0;title\x1b\\after")=="after","OSC with ST stripped");
  check(gv::sanitize("10%\r55%\r100% done")=="100% done","carriage returns keep the last state");
  check(gv::sanitize("a\tb\r\nc")=="a       b\nc","tabs expand and CRLF normalizes");
  check(gv::sanitize("bell\x07 and\x01 nul")=="bell and nul","control bytes dropped");

  std::string out;
  for(int i=1;i<=30;++i)out+="line "+std::to_string(i)+"\n";
  const auto shell=gv::shellView({{"stdout",out},{"stderr","oops\nworse"}},false);
  check(shell["lines"].size()+shell["errLines"].size()==gv::collapsedLines,
    "collapsed shell output fits the line budget");
  check(shell["total"]==30&&shell["errTotal"]==2&&shell["errLines"].size()==2,
    "standard error keeps its rows");
  const auto full=gv::shellView({{"stdout",out}},true);
  check(full["lines"].size()==30,"expanded shell output is complete");
  const auto quiet=gv::shellView({{"stdout",""},{"stderr",""},
    {"backgroundTaskId","b1"}},false);
  check(quiet["background"]=="b1","background shells say so");

  const Json patch=Json::array({{{"oldStart",3},{"newStart",3},
    {"lines",Json::array({" keep","-old","+new","+more"})}}});
  const auto diff=gv::diffView("Updated","src/a.cpp",patch,false);
  check(diff["summary"]=="Updated src/a.cpp with 2 additions and 1 removal",
    "diff summary counts additions and removals");
  check(gv::diffView("Updated","a",Json::array(),false)["summary"]=="No changes to a",
    "empty patches say nothing changed");
  const auto created=gv::createdFileView("notes.md","one\ntwo\n",false);
  check(created["summary"]=="Created notes.md (2 lines)"&&
    created["hunks"][0]["lines"][0]=="+one","created files show their first lines");
  const auto preview=gv::editPreview("a.cpp","a\nb\nc\n","a\nB\nc\n",10);
  const auto& lines=preview["hunks"][0]["lines"];
  check(lines==Json::array({" a","-b","+B"," c"}),"edit previews keep context around the change");

  const auto read=gv::readView({{"type","text"},{"file",{{"content","x\ny\n"},
    {"numLines",2},{"startLine",5},{"totalLines",100}}}},false);
  check(read["summary"]=="Read 2 lines (5–6 of 100)"&&read["start"]==5,
    "partial reads say which lines");
  check(gv::errorView("<tool_use_error>File missing</tool_use_error>",false)["lines"][0]==
    "File missing","errors drop their protocol tag");
  check(gv::visibleText(Json{{"content","shown"},{"other",1}})=="shown",
    "objects show their text member");
  check(gv::globView({{"filenames",Json::array({"a","b"})},{"numFiles",2}},false)["summary"]==
    "Found 2 files","glob summary");

  check(gv::displayPath("/home/b/roost/src/a.cpp","/home/b/roost","/home/b")=="src/a.cpp",
    "Roost-relative path");
  check(gv::displayPath("/home/b/other/x","/home/b/roost","/home/b")=="~/other/x",
    "home-relative path");
#ifndef _WIN32
  // Directory symlinks need extra privileges on Windows; the lexical cases
  // above cover that platform.
  const auto base=std::filesystem::temp_directory_path()/
    ("batbox-visual-path-"+std::to_string(std::random_device{}()));
  std::filesystem::create_directories(base/"real"/"src");
  std::filesystem::create_directory_symlink(base/"real",base/"link");
  const auto canonical=std::filesystem::canonical(base/"real").string();
  const bool relative=gv::displayPath((base/"link"/"src"/"a.cpp").string(),canonical,"")==
    "src/a.cpp";
  std::filesystem::remove_all(base);
  check(relative,"paths through a symlinked Roost stay relative");
#endif

  gv::GadgetViewStyle style;
  const auto rendered=draw(gv::projectGadgetResult(shell,style,60),60,16);
  check(rendered.find("  ⎿  line 1")!=std::string::npos,"results hang from the connector");
  check(rendered.find("… +20 lines (ctrl+o to expand)")!=std::string::npos,
    "truncated output offers the expand key");
}

void text(){
  check(batbox::terminal_ftxui::displayWidth("界a")==3,"fullwidth glyphs count twice");
  check(batbox::terminal_ftxui::truncateColumns("界界界",5)=="界界…",
    "truncation never splits a fullwidth glyph");
  check(batbox::terminal_ftxui::truncateColumns("short",10)=="short","short text is kept");
  const auto fitted=draw(ftxui::hbox({batbox::terminal_ftxui::fitText(
    "a long label that cannot fit"),ftxui::text("|")}),12,1);
  check(fitted.find("…|")!=std::string::npos,"fitText ends with an ellipsis when squeezed");
}

void highlight(){
  const md::SyntaxPalette palette{"<K>","<S>","<N>","<C>","<T>","<A>","<R>","<H>"};
  const auto cpp=md::highlightSyntax("int x = 42; // note","cpp",palette);
  check(cpp.find("<K>int")!=std::string::npos&&cpp.find("<N>42")!=std::string::npos&&
    cpp.find("<C>// note")!=std::string::npos,"C++ keywords, numbers and comments");
  const auto python=md::highlightSyntax("def f():\n  \"\"\"doc\nmore\"\"\"\n  return 'x'",
    "python",palette);
  check(python.find("<K>def")!=std::string::npos&&python.find("<S>\"\"\"doc\nmore\"\"\"")!=
    std::string::npos&&python.find("<S>'x'")!=std::string::npos,"Python strings span lines");
  const auto json=md::highlightSyntax("{\"key\": true, \"n\": 3}","json",palette);
  check(json.find("<T>\"key\"")!=std::string::npos&&json.find("<K>true")!=std::string::npos,
    "JSON keys and literals");
  const auto diff=md::highlightSyntax("@@ -1 +1 @@\n-old\n+new","diff",palette);
  check(diff.find("<R>-old")!=std::string::npos&&diff.find("<A>+new")!=std::string::npos,
    "diff lines");
  check(md::supportsSyntaxLanguage("TypeScript")&&!md::supportsSyntaxLanguage("cobol"),
    "language support");
  check(md::languageForPath("src/lib.rs")=="rs"&&md::languageForPath("notes.txt").empty(),
    "languages by file extension");
}

void markdownBlocks(){
  auto services=md::indigenousFormatServices(false,false,"",[](const std::string& value){
    return static_cast<int>(batbox::terminal_ftxui::displayWidth(value));});
  services.blockStyle=[]{md::FormatServices::BlockStyle style;style.readingWidth=40;
    return style;};
  const md::WrapAnsi wrap=[](const std::string& value,int,bool){return value;};
  md::Renderer renderer(services,wrap,60);
  const auto drawn=draw(renderer.render(
    "Intro [docs](https://example.com) here.\n\n- [x] done\n- [ ] open\n\n```py\nx = 1\n```\n\n---\n\nEnd"),
    60,14);
  check(drawn.find("docs (https://example.com)")!=std::string::npos,
    "links keep their words without hyperlink support");
  check(drawn.find("[✓] done")!=std::string::npos&&drawn.find("[ ] open")!=std::string::npos,
    "task list state");
  check(drawn.find("╭─ py ")!=std::string::npos&&drawn.find("│ x = 1")!=std::string::npos,
    "fenced code is framed with its language");
  check(drawn.find("────────────────────────────────────────")!=std::string::npos,
    "rules draw a line");
  check(drawn.find("\n\n\n\n")==std::string::npos,"blocks are separated by one blank row");
  // A styled word followed by a long plain run wraps between words, not
  // where the style changes.
  const auto flowed=draw(md::projectAnsi("A \x1b[1mterminal UI\x1b[22m (TUI) is a user "
    "interface that runs inside a text terminal"),30,4);
  check(flowed.find("A terminal UI (TUI) is a user")!=std::string::npos,
    "text after a styled run continues on the same row");
}

void themeCommand(){
  event_loop::Loop loop;
  std::optional<interactive::UiRequest> view;
  auto router=user_input::UiCommandRouter::create(loop,{
    [&](std::optional<interactive::UiRequest> next){view=std::move(next);},
    [](const std::string& failure){throw std::runtime_error(failure);},
    [](bool){},[](bool){}});
  std::string saved;
  auto theme=std::make_shared<ip::ThemeState>(ip::ThemeStateServices{
    []{return std::string("auto");},[&](const std::string& value){saved=value;},
    []{return std::string("dark");},[]{}});
  auto command=application_commands::makeThemeCommand(router,[theme]{return theme;},true);
  auto pending=command.invokeUi("");
  for(int i=0;i<10&&!view;++i)loop.runReady();
  check(view&&view->id=="theme"&&view->choices.size()==ip::selectableThemes().size(),
    "/theme lists every theme");
  check(view->choiceInfo.front().badge=="current","the saved theme is marked current");
  router->ui()->dispatchAction({interactive::UiAction::Kind::MoveSelection,2,"down"});
  check(theme->currentTheme()=="batbox-light"&&saved.empty(),
    "moving the selection previews without saving");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
  const auto cancelled=await(loop,pending);
  check(theme->currentTheme()=="batbox-dark"&&saved.empty()&&
    cancelled.result&&cancelled.result->find("unchanged")!=std::string::npos,
    "escape restores the current theme");
  auto direct=await(loop,command.invokeUi("monochrome"));
  check(saved=="monochrome"&&theme->currentTheme()=="monochrome",
    "/theme <name> saves the choice");
  auto unknown=await(loop,command.invokeUi("neon"));
  check(unknown.result&&unknown.result->find("Unknown theme")!=std::string::npos,
    "unknown names are explained");
  (void)direct;
}

void helpCommand(){
  event_loop::Loop loop;
  std::optional<interactive::UiRequest> view;
  auto router=user_input::UiCommandRouter::create(loop,{
    [&](std::optional<interactive::UiRequest> next){view=std::move(next);},
    [](const std::string& failure){throw std::runtime_error(failure);},
    [](bool){},[](bool){}});
  auto help=commands_indigenous::makeHelpCommand(router,[]{
    return std::vector<Json>{{{"name","theme"},{"description","Choose the color theme"}},
      {{"name","clear"},{"description","Clear the Flight"}},
      {{"name","zzz"},{"description","Something else"}},
      {{"name","secret"},{"isHidden",true}}};
  });
  auto pending=help.invokeUi("");
  for(int i=0;i<10&&!view;++i)loop.runReady();
  check(view&&view->choices.size()>=3&&view->choices[0]=="/clear"&&
    view->choiceInfo[0].section=="Flight"&&view->choiceInfo[1].section=="Model and settings"&&
    view->choiceInfo[2].section=="More","/help groups commands by section");
  for(const auto& choice:view->choices)check(choice!="/secret","hidden commands stay hidden");
  router->ui()->dispatchAction({interactive::UiAction::Kind::MoveSelection,1,"down"});
  check(view->selectedIndex==1,"help moves its selection");
  router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,1,{}});
  const auto chosen=await(loop,pending);
  check(chosen.nextInput&&*chosen.nextInput=="/theme "&&chosen.submitNextInput==false,
    "choosing a command puts it in the poetry input");
}
} // namespace

int main(){
  try{
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
    ip::applyColorDepth(ip::ColorDepth::TrueColor);
    colorDepth();themes();viewModel();text();highlight();markdownBlocks();
    themeCommand();helpCommand();
    std::cout<<"Visual roles, Gadget views, highlighting and theme commands pass\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
