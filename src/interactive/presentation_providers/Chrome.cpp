#include "interactive/presentation_providers/Chrome.hpp"
#include "interactive/presentation_providers/MissionFooter.hpp"
#include "runtime/Encoding.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace batbox::interactive::presentation_providers {
namespace {
using ViewState=conversation_shell::ViewState;
ftxui::Element empty(){return ftxui::vbox(ftxui::Elements{});}
ftxui::Element colored(std::string text,ftxui::Color color){
  return ftxui::text(value::encodeUtf8(text))|ftxui::color(color);
}
ftxui::Element coloredParagraph(std::string text,ftxui::Color color){
  return ftxui::paragraph(value::encodeUtf8(text))|ftxui::color(color);
}
std::string shortcut(const ChromeServices& imports,const std::string& action,
    const std::string& context,const std::string& fallback){
  return imports.shortcuts.display(action,context,fallback);
}
ftxui::Element activity(const std::string& theme,const ViewState& view){
  if(!view.remembering&&!view.presentation.loading)return empty();
  const double strength=(1.0+std::cos(
    static_cast<double>(view.activityFrame%16)*6.283185307179586/16.0))/2.0;
  const auto base=themeColor(theme,view.remembering?"remember":"suggestion");
  const bool simple=theme=="monochrome"||theme.ends_with("ansi");
  const auto color=simple?base:ftxui::Color::Interpolate(
    static_cast<float>(0.35+0.65*strength),themeColor(theme,"subtle"),base);
  const auto label=view.remembering?
    (view.columns<30?"remembering…":"I’m remembering…"):"working";
  auto result=colored(std::string("  ● ")+label,color);
  if(simple&&strength<0.4)result=result|ftxui::dim;
  return result;
}
ftxui::Element header(const ChromeServices& imports,const ViewState& view){
  const auto theme=imports.theme.currentTheme();
  std::vector<ftxui::Element> parts;
  parts.push_back(colored("Batbox",themeColor(theme,"brand"))|ftxui::bold);
  parts.push_back(activity(theme,view));
  if(view.columns<48){
    return ftxui::hbox(std::move(parts));
  }
  if(view.batId){
    parts.push_back(colored("  /  ",themeColor(theme,"subtle")));
    parts.push_back(colored(view.batLabel,themeColor(theme,"suggestion"))|ftxui::bold);
    parts.push_back(colored("  ["+view.batStatus+"]",themeColor(theme,
      view.presentation.loading?"brand":"subtle")));
  }else parts.push_back(colored("  Leader",themeColor(theme,"subtle")));
  const auto model=imports.modelLabel();
  if(!model.empty())parts.push_back(colored("  "+model,
    themeColor(theme,"subtle")));
  const auto mode=imports.permissionMode();
  if(!mode.empty())parts.push_back(colored("  ["+mode+"]",
    themeColor(theme,"permission")));
  return ftxui::hbox(std::move(parts));
}
ftxui::Element footer(const ChromeServices& imports,const ViewState& view){
  const auto theme=imports.theme.currentTheme();
  const auto subtle=themeColor(theme,"subtle");
  std::vector<ftxui::Element> parts;
  const auto status=imports.statusText();
  if(!status.empty())parts.push_back(colored(status,subtle));
  if(view.searchOpen){
    const auto count=std::to_string(view.searchCurrent)+"/"+
      std::to_string(view.searchCount);
    parts.push_back(colored((parts.empty()?"":"  ·  ")+
      std::string(view.searchQuery)+" "+count,subtle));
  }else{
    if(view.batId){
      parts.push_back(colored("Follow-up → Bat · /leader, Esc or Alt+← to return · Ctrl+T Bats",
        themeColor(theme,"suggestion")));
      if(view.batStatus=="completed"||view.batStatus=="killed"||view.batStatus=="failed")
        parts.push_back(colored(" · sending resumes this Bat",subtle));
      return ftxui::hbox(std::move(parts));
    }
    const auto dir=imports.workingDirectory();
    if(!dir.empty())parts.push_back(colored((parts.empty()?"":"  ·  ")+dir,
      subtle));
    const auto key=shortcut(imports,"app:toggleTranscript","Global","ctrl+o");
    parts.push_back(colored("  ·  "+key+" transcript",subtle));
  }
  return ftxui::hbox(std::move(parts));
}
std::string clippedLine(const std::string& text,int columns,bool& clipped){
  if(columns<=0){clipped=clipped||!text.empty();return {};}
  if(ftxui::string_width(text)<=columns)return text;
  clipped=true;
  std::string result;
  int width=0;
  for(const auto& glyph:ftxui::Utf8ToGlyphs(text)){
    const int next=ftxui::string_width(glyph);
    if(width+next>columns-1)break;
    result+=glyph;width+=next;
  }
  return result+"…";
}
ftxui::Element boundedModal(const ChromeServices& imports,const ViewState& view){
  const auto& dialog=*view.presentation.modal;
  const auto theme=imports.theme.currentTheme();
  const int available=std::max(1,view.rows-(view.rows>5?1:0));
  const int choices=static_cast<int>(dialog.choices.size());
  const bool border=available>=choices+3&&view.columns>=4;
  const int width=std::max(1,view.columns-(border?2:0));
  const int innerRows=available-(border?2:0);
  const bool title=!dialog.title.empty()&&innerRows>choices;
  const int bodyRows=std::max(0,innerRows-choices-(title?1:0));
  std::vector<std::string> preview;
  std::size_t cursor=0;
  while(cursor<dialog.body.size()&&preview.size()<*dialog.bodyPreviewLines){
    const auto end=dialog.body.find('\n',cursor);
    auto line=dialog.body.substr(cursor,end==std::string::npos?end:end-cursor);
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    preview.push_back(std::move(line));
    cursor=end==std::string::npos?dialog.body.size():end+1;
  }
  const bool more=cursor<dialog.body.size()||static_cast<int>(preview.size())>bodyRows;
  const int shown=std::min(static_cast<int>(preview.size()),
    std::max(0,bodyRows-(more?1:0)));
  ftxui::Elements lines;
  if(title){
    bool clipped=false;
    const auto label=(bodyRows==0&&!dialog.body.empty()?"… ":"")+dialog.title;
    lines.push_back(colored(clippedLine(label,width,clipped),
      themeColor(theme,"brand"))|ftxui::bold);
  }
  for(int index=0;index<shown;++index){
    bool clipped=false;
    lines.push_back(colored(clippedLine(preview[static_cast<std::size_t>(index)],
      width,clipped),themeColor(theme,"text")));
  }
  if(more&&bodyRows>0){
    bool clipped=false;
    lines.push_back(colored(clippedLine("… more details",width,clipped),
      themeColor(theme,"inactive")));
  }
  // Extremely short terminals keep the selected action in the visible window.
  const auto count=std::min(dialog.choices.size(),static_cast<std::size_t>(innerRows));
  const auto first=dialog.selectedIndex>=count&&count>0?dialog.selectedIndex-count+1:0;
  for(std::size_t offset=0;offset<count;++offset){
    const auto index=first+offset;
    const bool selected=index==dialog.selectedIndex;
    bool clipped=false;
    auto line=colored(clippedLine(std::string(selected?"› ":"  ")+
      dialog.choices[index],width,clipped),
      themeColor(theme,selected?"suggestion":"text"));
    if(selected)line=line|ftxui::bold;
    lines.push_back(std::move(line));
  }
  auto result=ftxui::vbox(std::move(lines));
  return border?result|ftxui::border:result;
}
ftxui::Element modal(const ChromeServices& imports,const ViewState& view){
  const auto& dialog=view.presentation.modal;
  if(!dialog)return empty();
  if(dialog->bodyPreviewLines)return boundedModal(imports,view);
  const auto theme=imports.theme.currentTheme();
  std::vector<ftxui::Element> lines;
  if(!dialog->title.empty())
    lines.push_back(colored(dialog->title,themeColor(theme,"brand"))|
      ftxui::bold);
  if(!dialog->body.empty()){
    std::size_t start=0;
    while(start<dialog->body.size()){
      const auto end=dialog->body.find('\n',start);
      lines.push_back(coloredParagraph(dialog->body.substr(start,
        end==std::string::npos?end:end-start),themeColor(theme,"text")));
      if(end==std::string::npos)break;
      start=end+1;
    }
  }
  for(std::size_t i=0;i<dialog->choices.size();++i){
    const auto selected=i==dialog->selectedIndex;
    auto line=colored(std::string(selected?"› ":"  ")+dialog->choices[i],
      themeColor(theme,selected?"suggestion":"text"));
    if(selected)line=line|ftxui::bold;
    lines.push_back(std::move(line));
  }
  if(dialog->kind==interactive::UiRequest::Kind::Form&&dialog->sensitiveInput)
    lines.push_back(colored("Private input · hidden from poetry history",
      themeColor(theme,"subtle")));
  if(dialog->kind==interactive::UiRequest::Kind::Notice)
    lines.push_back(colored("Enter or Esc to close",themeColor(theme,"subtle")));
  return ftxui::vbox(std::move(lines))|ftxui::border;
}
ftxui::Element pill(const ChromeServices& imports,const ViewState& view){
  if(!view.pillVisible)return empty();
  return ftxui::hbox({ftxui::filler(),
    colored("[ Go to the action ↓ ]",
      themeColor(imports.theme.currentTheme(),"suggestion"))|ftxui::bold,
    ftxui::filler()});
}
ftxui::Element logo(const ChromeServices& imports){
  return colored("Batbox",themeColor(imports.theme.currentTheme(),"brand"))|
    ftxui::bold;
}
ftxui::Element welcome(const ChromeServices& imports,const ViewState& view){
  const auto theme=imports.theme.currentTheme();
  const bool simple=theme=="monochrome"||theme.ends_with("ansi");
  const bool light=theme.starts_with("light");
  const auto purple=simple?themeColor(theme,"merged"):
    ftxui::Color::RGB(light?116:193,light?55:139,light?163:236);
  const auto cream=simple?themeColor(theme,"text"):
    (light?ftxui::Color::RGB(122,84,43):ftxui::Color::RGB(255,237,197));
  const auto orange=simple?themeColor(theme,"brand"):
    ftxui::Color::RGB(light?183:255,light?84:161,light?26:74);
  const auto mint=simple?themeColor(theme,"planMode"):
    (light?ftxui::Color::RGB(33,122,103):ftxui::Color::RGB(153,229,199));
  const auto frame=std::min(view.welcomeFrame,
    conversation_shell::welcomeAnimationFrames);
  const bool moving=frame<conversation_shell::welcomeAnimationFrames;
  const int available=std::max(1,view.bodyRows?view.bodyRows:view.rows);
  ftxui::Element art;
  if(view.columns<24||available<7){
    // Tiny terminals still introduce both companions without wide emoji cells.
    if(view.columns>=21)
      art=ftxui::hbox({colored("/\\^/\\",purple),ftxui::text("  "),
        colored("(o o)~",cream),ftxui::text("  "),colored("Batbox",orange)});
    else if(view.columns>=12)
      art=ftxui::hbox({colored("/\\^/\\",purple),ftxui::text(" "),colored("(o o)~",cream)});
    else if(view.columns>=9)
      art=ftxui::hbox({colored("/\\^/\\",purple),colored("(oo)",cream)});
    else if(view.columns>=8)
      art=ftxui::hbox({colored("^v^",purple),colored("(oo)~",cream)});
    else if(view.columns>=2)
      art=ftxui::hbox({colored("B",purple),colored("G",cream)});
    else art=available>1?ftxui::vbox({colored("B",purple),colored("G",cream)}):
      colored("B",purple);
  }else{
    const bool large=view.columns>=48&&available>=13;
    const bool flap=moving&&((frame/2)%2==0);
    const int batOffset=moving&&(frame<2||(frame>=6&&frame<8))?1:0;
    const int ghostOffset=moving&&(frame<4||(frame>=8&&frame<10))?1:0;
    const std::vector<std::string> bat=large?std::vector<std::string>{
      R"(        /\       /\)",
      flap?R"(   /\  /  \_____/  \  /\)":R"(   __  /  \_____/  \  __)",
      flap?R"(  /  \/   (o.o)    \/  \)":R"(  /  \    (o.o)    /  \)",
      R"( /         \v v/         \)",
      R"( \_/\_/\____\_/____/\_/\_/)",
      R"(          /   \)",
      R"(         (_/ \_))"}:
      std::vector<std::string>{
        R"(  /\       /\)",
        flap?R"( /  \ (o.o)/  \)":R"( <  \ (o.o)/  >)",
        R"(  \__\ v v/__/)",
        R"(     (___))"};
    const std::vector<std::string> ghost=large?std::vector<std::string>{
      R"(    .----.)",
      R"(   / (o o)\)",
      R"(  /    u   \)",
      R"( (  |    |  ))",
      R"(  \_|    |_/)",
      R"(    |    |)",
      R"(    '~~~~')"}:
      std::vector<std::string>{
        R"( .----.)",
        R"( /(o o)\)",
        R"( |  u  |)",
        R"( '~~~~~')"};
    const int batWidth=large?27:15,ghostWidth=large?14:9;
    const int gap=large?3:0;
    ftxui::Elements lines;
    // Both poses occupy the same stage, so wingbeats cannot move the transcript.
    for(int row=0;row<(large?8:5);++row){
      const auto line=[row](const std::vector<std::string>& sprite,int offset){
        const int index=row-offset;
        return index>=0&&index<static_cast<int>(sprite.size())?
          sprite[static_cast<std::size_t>(index)]:std::string{};
      };
      lines.push_back(ftxui::hbox({
        colored(line(bat,batOffset),purple)|ftxui::size(ftxui::WIDTH,ftxui::EQUAL,batWidth),
        ftxui::text(std::string(gap,' ')),
        colored(line(ghost,ghostOffset),cream)|ftxui::size(ftxui::WIDTH,ftxui::EQUAL,ghostWidth)}));
    }
    lines.push_back(ftxui::hbox({colored("  *  ",orange),
      colored("Batbox",cream)|ftxui::bold,colored("  *  ",orange)})|ftxui::hcenter);
    if(available>=9)lines.push_back(colored("code after dark",mint)|ftxui::hcenter);
    art=ftxui::vbox(std::move(lines));
  }
  art->ComputeRequirement();
  const int height=art->requirement().min_y;
  const int bottom=height<available?1:0;
  const int top=std::clamp(view.rows/3-view.bodyScreenRow,0,
    std::max(0,available-height-bottom-1));
  ftxui::Elements layout;
  if(top>0)layout.push_back(ftxui::text("")|
    ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,top));
  layout.push_back(std::move(art)|ftxui::hcenter);
  if(bottom)layout.push_back(ftxui::text(""));
  return ftxui::vbox(std::move(layout));
}
ftxui::Element divider(const ChromeServices& imports,
    const message_list_view::DividerRequest& request){
  std::string label;
  switch(request.kind){
    case message_list_view::DividerKind::ShowPrevious:
      label=shortcut(imports,"transcript:toggleShowAll","Transcript",
        "ctrl+e")+" to show "+std::to_string(request.count)+
        " previous messages";break;
    case message_list_view::DividerKind::HidePrevious:
      label=shortcut(imports,"transcript:toggleShowAll","Transcript",
        "ctrl+e")+" to hide "+std::to_string(request.count)+
        " previous messages";break;
    case message_list_view::DividerKind::Unseen:
      label=std::to_string(request.count)+" new messages";break;
  }
  const auto remaining=std::max(0,request.columns-
    static_cast<int>(label.size())-2);
  const auto left=remaining/2,right=remaining-left;
  std::string text;
  for(int i=0;i<left;++i)text+="─";
  text+=" "+label+" ";
  for(int i=0;i<right;++i)text+="─";
  return colored(std::move(text),
    themeColor(imports.theme.currentTheme(),"subtle"));
}
} // namespace

conversation_shell::ChromeProviders makeChromeProviders(ChromeServices imports){
  if(!imports.modelLabel||!imports.workingDirectory||
     !imports.permissionMode||!imports.statusText||
     !imports.shortcuts.loadBindings)
    throw std::invalid_argument("Chrome providers require live selectors");
  conversation_shell::ChromeProviders result;
  result.renderHeader=[imports](const auto& view){return header(imports,view);};
  result.renderFooter=[imports](const auto& view){return footer(imports,view);};
  result.renderModal=[imports](const auto& view){return modal(imports,view);};
  result.renderPill=[imports](const auto& view){return pill(imports,view);};
  result.renderMissions=[imports](const auto& view){
    return missionFooter(view.backgroundMissions,view.columns,view.rows,
      themeColor(imports.theme.currentTheme(),"suggestion"));
  };
  result.renderLogo=[imports]{return logo(imports);};
  result.renderWelcome=[imports](const auto& view){return welcome(imports,view);};
  result.renderDivider=[imports](const auto& request){
    return divider(imports,request);
  };
  return result;
}
} // namespace batbox::interactive::presentation_providers
