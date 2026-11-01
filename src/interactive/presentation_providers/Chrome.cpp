#include "interactive/presentation_providers/Chrome.hpp"
#include "interactive/presentation_providers/MissionFooter.hpp"
#include "interactive/presentation_providers/CardPalettes.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "terminal_ftxui/TextFit.hpp"
#include "runtime/Encoding.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <utility>

#ifndef BATBOX_VERSION
#define BATBOX_VERSION ""
#endif

namespace batbox::interactive::presentation_providers {
namespace {
using ViewState=conversation_shell::ViewState;
using terminal_ftxui::displayWidth;
using terminal_ftxui::fitText;
using terminal_ftxui::truncateColumns;
ftxui::Element empty(){return ftxui::vbox(ftxui::Elements{});}
ftxui::Element colored(std::string text,ftxui::Color color){
  return ftxui::text(value::encodeUtf8(text))|ftxui::color(color);
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
// Roles paint the chrome; monochrome themes resolve every role to the
// terminal default, so attributes carry the hierarchy there.
ftxui::Element tone(ftxui::Element element,ftxui::Color color,
    const StyleRoles& roles,bool quiet=false){
  if(roles.monochrome)return quiet?element|ftxui::dim:element;
  return element|ftxui::color(color);
}
ftxui::Element header(const ChromeServices& imports,const ViewState& view){
  const auto theme=imports.theme.currentTheme();
  const auto& roles=styleRoles(theme);
  std::vector<ftxui::Element> parts;
  parts.push_back(tone(ftxui::text("✦ "),roles.mint,roles));
  parts.push_back(colored("Batbox",roles.brand)|ftxui::bold);
  if(view.columns>=48){
    if(view.batId){
      parts.push_back(tone(ftxui::text("  ›  "),roles.faint,roles,true));
      parts.push_back(colored(view.batLabel,roles.cream)|ftxui::bold);
      parts.push_back(tone(ftxui::text("  "+view.batStatus),
        view.presentation.loading?roles.spinner:roles.muted,roles,true));
    }else parts.push_back(tone(ftxui::text("  Leader"),roles.muted,roles,true));
  }
  // Turn activity lives in the status row; recovery happens before a turn.
  if(view.remembering){
    parts.push_back(ftxui::filler());
    parts.push_back(activity(theme,view));
  }
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
      parts.push_back(colored("Follow-up → Bat · /leader or Esc to return",
        themeColor(theme,"suggestion")));
      if(view.batStatus=="completed"||view.batStatus=="stopped"||view.batStatus=="failed")
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
std::string spinnerGlyph(unsigned frame){
  static const char* frames[]={"·","⋆","✧","✦","✶","✦","✧","⋆"};
  return frames[frame%8];
}
std::string workingVerb(const std::string& mode){
  if(mode=="requesting")return "Connecting…";
  if(mode=="thinking")return "Thinking…";
  if(mode=="responding")return "Writing…";
  if(mode=="tool-input")return "Preparing a Gadget…";
  if(mode=="tool-use")return "Running Gadgets…";
  return "Working…";
}
std::string elapsed(double ms){
  const auto seconds=static_cast<long long>(std::max(0.0,ms)/1000.0);
  if(seconds<60)return std::to_string(seconds)+"s";
  const auto minutes=seconds/60,rest=seconds%60;
  return std::to_string(minutes)+"m "+(rest<10?"0":"")+std::to_string(rest)+"s";
}
std::string homePath(const std::string& path,const std::string& home){
  if(home.empty()||path.size()<home.size()||path.compare(0,home.size(),home)!=0)
    return path;
  if(path.size()==home.size())return "~";
  if(path[home.size()]!='/'&&path[home.size()]!='\\')return path;
  return "~"+path.substr(home.size());
}
// The final directories carry the meaning; the root of a long path does not.
std::string tailPath(const std::string& path,int width){
  if(displayWidth(path)<=width)return path;
  auto rest=path;
  while(true){
    const auto slash=rest.find_first_of("/\\",1);
    if(slash==std::string::npos)break;
    rest=rest.substr(slash);
    if(displayWidth("…"+rest)<=width)return "…"+rest;
  }
  return truncateColumns(path,width);
}
struct Badge {std::string text;ftxui::Color color;bool strong=false;};
Badge modeBadge(const std::string& mode,const StyleRoles& roles){
  if(mode=="plan")return {"◆ plan",roles.modePlan,true};
  if(mode=="acceptEdits")return {"◆ accept edits",roles.modeAccept,true};
  if(mode=="bypassPermissions")return {"◆ yolo",roles.modeYolo,true};
  if(mode=="dontAsk")return {"◆ don't ask",roles.statusWarn,true};
  if(mode=="auto")return {"◆ auto",roles.modeAccept,true};
  return {"◇ "+(mode.empty()?std::string("default"):mode),roles.modeDefault,false};
}
struct Segment {
  std::string text;
  ftxui::Color color;
  bool bold=false,quiet=false;
};
int segmentsWidth(const std::vector<Segment>& segments,int separator){
  int width=0;
  for(const auto& segment:segments)width+=displayWidth(segment.text);
  return width+std::max(0,static_cast<int>(segments.size())-1)*separator;
}
ftxui::Element joinSegments(const std::vector<Segment>& segments,const StyleRoles& roles,
    const std::string& separator){
  ftxui::Elements parts;
  for(std::size_t i=0;i<segments.size();++i){
    if(i)parts.push_back(tone(ftxui::text(separator),roles.faint,roles,true));
    auto part=tone(ftxui::text(segments[i].text),segments[i].color,roles,segments[i].quiet);
    if(segments[i].bold)part=part|ftxui::bold;
    parts.push_back(std::move(part));
  }
  return ftxui::hbox(std::move(parts));
}
ftxui::Color statusTone(const std::string& text,const StyleRoles& roles){
  std::string lower=text;
  std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){
    return static_cast<char>(std::tolower(ch));});
  if(lower.starts_with("error")||lower.find("failed")!=std::string::npos||
     lower.find("could not")!=std::string::npos||lower.find("cannot")!=std::string::npos)
    return roles.statusErr;
  if(lower.starts_with("warning"))return roles.statusWarn;
  return roles.muted;
}
std::size_t runningBats(const Json& missions){
  std::size_t count=0;
  if(missions.is_array())for(const auto& mission:missions){
    const auto& type=value::property(mission,"type");
    if(type=="local_agent"||value::property(mission,"agentType").is_string())++count;
  }
  return count;
}
ftxui::Element statusRow(const ChromeServices& imports,const ViewState& view){
  // A Bat conversation keeps its follow-up guidance from the classic footer.
  if(view.batId)return footer(imports,view);
  const auto& roles=styleRoles(imports.theme.currentTheme());
  const int columns=std::max(1,view.columns);
  const std::string separator=" · ";
  std::vector<Segment> right;
  const auto status=imports.statusText();
  if(view.searchOpen){
    right.push_back({"/"+std::string(view.searchQuery),roles.text});
    right.push_back({std::to_string(view.searchCurrent)+"/"+
      std::to_string(view.searchCount),roles.muted});
  }else{
    // Notices and errors come first and stay until they clear.
    if(!status.empty())right.push_back({status,statusTone(status,roles)});
    if(view.presentation.loading){
      right.push_back({spinnerGlyph(view.activityFrame)+" "+workingVerb(view.streamMode),
        roles.spinner,true});
      right.push_back({elapsed(view.workingMs),roles.muted,false,true});
      right.push_back({shortcut(imports,"chat:cancel","Chat","esc")+" to interrupt",
        roles.muted,false,true});
    }else if(status.empty()){
      if(imports.contextUsage)if(const auto usage=imports.contextUsage()){
        const auto percent=static_cast<int>(std::round(std::clamp(*usage,0.0,1.0)*100));
        right.push_back({std::to_string(percent)+"% context",
          percent>=95?roles.statusErr:percent>=80?roles.statusWarn:roles.muted,false,
          percent<80});
      }
      if(const auto bats=runningBats(view.backgroundMissions))
        right.push_back({std::to_string(bats)+(bats==1?" Bat running":" Bats running"),
          roles.brand});
      right.push_back({shortcut(imports,"app:toggleTranscript","Global","ctrl+o")+
        " transcript",roles.muted,false,true});
    }
  }
  // The right side may take most of the row; the left gives up detail first.
  // Idle hints are the exception: they yield before the Roost details do.
  const int rightLimit=std::max(24,columns*3/5);
  while(right.size()>1&&segmentsWidth(right,3)>rightLimit)right.pop_back();
  if(!view.presentation.loading&&status.empty()&&!view.searchOpen){
    const auto mode=modeBadge(imports.permissionMode(),roles).text;
    const auto model=imports.modelLabel();
    const auto home=imports.homeDirectory?imports.homeDirectory():std::string{};
    const auto directory=homePath(imports.workingDirectory(),home);
    const auto branch=imports.gitBranch?imports.gitBranch():std::string{};
    std::vector<Segment> wanted{{mode,{}}};
    if(!model.empty())wanted.push_back({model,{}});
    if(!directory.empty())wanted.push_back({tailPath(directory,24),{}});
    if(!branch.empty())wanted.push_back({"⎇ "+truncateColumns(branch,20),{}});
    while(right.size()>1&&segmentsWidth(wanted,3)+segmentsWidth(right,3)+4>columns)
      right.pop_back();
  }
  if(!right.empty()&&segmentsWidth(right,3)>rightLimit)
    right.front().text=truncateColumns(right.front().text,rightLimit);
  const int rightWidth=segmentsWidth(right,3);
  const int leftBudget=std::max(0,columns-rightWidth-(right.empty()?0:2)-2);
  const auto badge=modeBadge(imports.permissionMode(),roles);
  std::vector<Segment> left{{badge.text,badge.color,badge.strong,!badge.strong}};
  const auto model=imports.modelLabel();
  const auto home=imports.homeDirectory?imports.homeDirectory():std::string{};
  auto directory=homePath(imports.workingDirectory(),home);
  const auto branch=imports.gitBranch?imports.gitBranch():std::string{};
  // Give up detail in order: the Roost's leading directories, the branch
  // name's tail, the branch, the Roost, then the model name's tail.
  const auto fixedWidth=[&](bool withDirectory,bool withBranch,int branchWidth){
    std::vector<Segment> trial=left;
    if(!model.empty())trial.push_back({model,{}});
    if(withDirectory)trial.push_back({"",{}});
    if(withBranch)trial.push_back({truncateColumns("⎇ "+branch,branchWidth),{}});
    return segmentsWidth(trial,3);
  };
  bool showDirectory=!directory.empty(),showBranch=!branch.empty();
  int branchWidth=showBranch?displayWidth("⎇ "+branch):0;
  int directoryRoom=leftBudget-fixedWidth(showDirectory,showBranch,branchWidth);
  if(showBranch&&directoryRoom<12){
    branchWidth=std::min(branchWidth,20);
    directoryRoom=leftBudget-fixedWidth(showDirectory,true,branchWidth);
  }
  if(showBranch&&directoryRoom<12){
    showBranch=false;
    directoryRoom=leftBudget-fixedWidth(showDirectory,false,0);
  }
  if(showDirectory&&directoryRoom<8)showDirectory=false;
  if(!model.empty())left.push_back({model,roles.cream});
  if(showDirectory)left.push_back({tailPath(directory,directoryRoom),roles.muted,false,true});
  if(showBranch)left.push_back({truncateColumns("⎇ "+branch,branchWidth),roles.muted,false,true});
  const auto fits=[&]{return segmentsWidth(left,3)<=leftBudget;};
  if(!fits()&&left.size()>1){
    const int room=leftBudget-displayWidth(left.front().text)-3;
    if(room>=6)left[1].text=truncateColumns(left[1].text,room);
    else left.resize(1);
  }
  if(!fits())left.front().text=truncateColumns(left.front().text,std::max(1,leftBudget));
  ftxui::Elements row{ftxui::text(" "),joinSegments(left,roles,separator),ftxui::filler()};
  if(!right.empty()){
    row.push_back(joinSegments(right,roles," · "));
    row.push_back(ftxui::text(" "));
  }
  return ftxui::hbox(std::move(row));
}
conversation_shell::PoetryDecor poetryDecor(const ChromeServices& imports,
    const ViewState& view){
  const auto& roles=styleRoles(imports.theme.currentTheme());
  conversation_shell::PoetryDecor decor;
  decor.framed=true;
  decor.prompt=roles.userBar;
  decor.placeholderColor=roles.muted;
  decor.border=view.presentation.input.focused?roles.borderFocus:roles.border;
  if(view.searchOpen){
    decor.prompt=roles.mint;
    decor.placeholder="Search the transcript…";
  }else if(view.batId){
    decor.prompt=roles.mint;
    decor.placeholder="Follow up with this Bat… (esc returns to the leader)";
  }else decor.placeholder="Give your model a destination… (/help)";
  return decor;
}
ftxui::Element suggestionRows(const std::vector<application_completion::Suggestion>& items,
    std::size_t selected,const ViewState& view,const ChromeServices& imports){
  const auto& roles=styleRoles(imports.theme.currentTheme());
  const auto shown=std::min<std::size_t>(6,items.size());
  const auto first=selected>=shown?selected-shown+1:0;
  int nameWidth=0;
  for(std::size_t i=first;i<first+shown&&i<items.size();++i)
    nameWidth=std::max(nameWidth,displayWidth(items[i].display));
  nameWidth=std::min(nameWidth,std::max(8,view.columns/2));
  ftxui::Elements rows;
  for(std::size_t offset=0;offset<shown;++offset){
    const auto index=first+offset;
    if(index>=items.size())break;
    const auto& item=items[index];
    const bool active=index==selected;
    auto name=truncateColumns(item.display,nameWidth);
    name+=std::string(static_cast<std::size_t>(std::max(0,nameWidth-displayWidth(name))),' ');
    ftxui::Elements cells{
      tone(ftxui::text(active?" ❯ ":"   "),roles.selection,roles),
      tone(ftxui::text(name),active?roles.selection:roles.text,roles)};
    if(active)cells.back()=cells.back()|ftxui::bold;
    if(!item.description.empty()){
      cells.push_back(ftxui::text("  "));
      cells.push_back(tone(fitText(item.description),roles.muted,roles,true));
    }
    if(items.size()>shown&&offset+1==shown){
      cells.push_back(ftxui::filler());
      cells.push_back(tone(ftxui::text(" "+std::to_string(selected+1)+"/"+
        std::to_string(items.size())+" "),roles.faint,roles,true));
    }
    auto row=ftxui::hbox(std::move(cells));
    if(active&&!isDefault(roles.selectionBg))row=row|ftxui::bgcolor(roles.selectionBg);
    rows.push_back(std::move(row));
  }
  return ftxui::vbox(std::move(rows));
}

// Word wrap at display columns; words wider than a row are split.
std::vector<std::string> wrapWords(const std::string& line,int width){
  std::vector<std::string> rows;
  if(width<=0)return {line};
  std::string current;
  int used=0;
  std::size_t start=0;
  const auto flush=[&]{rows.push_back(current);current.clear();used=0;};
  while(start<=line.size()){
    const auto end=line.find(' ',start);
    auto word=line.substr(start,end==std::string::npos?std::string::npos:end-start);
    int wordWidth=displayWidth(word);
    while(wordWidth>width){
      if(used>0)flush();
      const auto head=truncateColumns(word,width,"");
      rows.push_back(head);
      word=word.substr(head.size());
      wordWidth=displayWidth(word);
    }
    if(used>0&&used+1+wordWidth>width)flush();
    if(used>0){current+=' ';++used;}
    current+=word;used+=wordWidth;
    if(end==std::string::npos)break;
    start=end+1;
  }
  if(!current.empty()||rows.empty())rows.push_back(current);
  return rows;
}
std::vector<std::string> splitBody(const std::string& body){
  std::vector<std::string> lines;
  std::size_t cursor=0;
  while(cursor<body.size()){
    const auto end=body.find('\n',cursor);
    auto line=body.substr(cursor,end==std::string::npos?end:end-cursor);
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    lines.push_back(std::move(line));
    cursor=end==std::string::npos?body.size():end+1;
  }
  return lines;
}
// "↑↓ choose · enter apply": the first word of each part is the key.
ftxui::Element hintRow(const std::string& hint,const StyleRoles& roles){
  ftxui::Elements parts;
  const std::string separator=" · ";
  std::size_t start=0;
  while(start<=hint.size()){
    const auto end=hint.find(separator,start);
    const auto part=hint.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(!parts.empty())parts.push_back(tone(ftxui::text(" · "),roles.faint,roles,true));
    const auto space=part.find(' ');
    parts.push_back(tone(ftxui::text(part.substr(0,space)),roles.kbd,roles)|ftxui::bold);
    if(space!=std::string::npos)
      parts.push_back(tone(ftxui::text(part.substr(space)),roles.muted,roles,true));
    if(end==std::string::npos)break;
    start=end+separator.size();
  }
  return ftxui::hbox(std::move(parts));
}
std::string defaultHint(const interactive::UiRequest& dialog){
  using Kind=interactive::UiRequest::Kind;
  if(!dialog.hint.empty())return dialog.hint;
  switch(dialog.kind){
    case Kind::Notice:return "enter close · esc close";
    case Kind::Form:return "enter submit · esc cancel";
    case Kind::Progress:return "esc cancel";
    case Kind::Select:
    case Kind::Confirm:return dialog.choices.size()>1?
      "↑↓ choose · enter select · esc cancel":"enter select · esc cancel";
  }
  return {};
}
struct ChoiceRow {
  ftxui::Element element;
  std::optional<std::size_t> choice;
};
ftxui::Element dialog(const ChromeServices& imports,const ViewState& view){
  if(!view.presentation.modal)return empty();
  const auto& request=*view.presentation.modal;
  const auto& roles=styleRoles(imports.theme.currentTheme());
  const bool bounded=request.bodyPreviewLines.has_value();
  const int columns=std::max(1,view.columns);
  const int maxRows=std::max(1,bounded?view.rows-(view.rows>5?1:0):view.rows-4);
  const std::size_t choiceCount=request.choices.size();
  const std::size_t selected=choiceCount?std::min(request.selectedIndex,choiceCount-1):0;
  const bool framed=columns>=20&&maxRows>=static_cast<int>(std::min<std::size_t>(choiceCount,4))+3;
  const int inner=std::max(1,columns-(framed?4:0));
  int rowsLeft=maxRows-(framed?2:0);

  // Choices: label rows, optional detail rows and section headings. Short
  // labels put their detail in an aligned second column.
  int labelWidth=0,badgeWidth=0;
  bool anyDetail=false;
  for(std::size_t i=0;i<choiceCount;++i){
    labelWidth=std::max(labelWidth,displayWidth(request.choices[i]));
    if(i<request.choiceInfo.size()){
      if(!request.choiceInfo[i].detail.empty())anyDetail=true;
      badgeWidth=std::max(badgeWidth,displayWidth(request.choiceInfo[i].badge));
    }
  }
  const bool columnsLayout=anyDetail&&labelWidth<=std::max(12,inner/3);
  std::vector<ChoiceRow> choiceRows;
  std::vector<std::size_t> labelRow(choiceCount,0);
  std::string section;
  for(std::size_t i=0;i<choiceCount;++i){
    const auto info=i<request.choiceInfo.size()?request.choiceInfo[i]:interactive::UiChoice{};
    if(!info.section.empty()&&info.section!=section){
      if(!choiceRows.empty())choiceRows.push_back({ftxui::text(""),{}});
      choiceRows.push_back({tone(fitText(info.section),roles.muted,roles,true)|ftxui::bold,{}});
      section=info.section;
    }
    const bool active=i==selected;
    const auto label=value::encodeUtf8(request.choices[i]);
    ftxui::Elements cells{
      tone(ftxui::text(active?"❯ ":"  "),roles.selection,roles)|ftxui::bold};
    if(columnsLayout){
      auto padded=label+std::string(static_cast<std::size_t>(
        std::max(0,labelWidth-displayWidth(label))),' ');
      cells.push_back(active?tone(ftxui::text(padded),roles.selection,roles)|ftxui::bold:
        tone(ftxui::text(padded),roles.text,roles));
    }else cells.push_back(active?tone(fitText(label),roles.selection,roles)|ftxui::bold:
      tone(fitText(label),roles.text,roles));
    if(columnsLayout&&badgeWidth>0){
      // Badges get their own column so details stay aligned.
      cells.push_back(ftxui::text("  "));
      cells.push_back(tone(ftxui::text(info.badge+std::string(static_cast<std::size_t>(
        std::max(0,badgeWidth-displayWidth(info.badge))),' ')),roles.mint,roles));
    }else if(!info.badge.empty()){
      cells.push_back(ftxui::text("  "));
      cells.push_back(tone(ftxui::text(info.badge),roles.mint,roles));
    }
    if(columnsLayout&&!info.detail.empty()){
      cells.push_back(ftxui::text("   "));
      cells.push_back(tone(fitText(info.detail),roles.muted,roles,true));
    }
    auto row=ftxui::hbox(std::move(cells));
    const bool bar=active&&!isDefault(roles.selectionBg);
    if(bar)row=row|ftxui::bgcolor(roles.selectionBg);
    labelRow[i]=choiceRows.size();
    choiceRows.push_back({std::move(row),i});
    if(!columnsLayout&&!info.detail.empty()){
      auto detail=ftxui::hbox({ftxui::text("    "),
        tone(fitText(info.detail),roles.muted,roles,true)});
      if(bar)detail=detail|ftxui::bgcolor(roles.selectionBg);
      choiceRows.push_back({std::move(detail),i});
    }
  }
  const auto hint=defaultHint(request);
  const bool sensitive=request.kind==interactive::UiRequest::Kind::Form&&
    request.sensitiveInput;
  // Choices come first: the selected action must stay reachable on tiny screens.
  // A long list on a roomy screen still leaves the request's first line, a
  // gap and the key hints visible.
  const int choiceNeed=static_cast<int>(choiceRows.size());
  const bool hasBody=!request.body.empty()||!request.preview.is_null();
  const int reserve=maxRows>=10?(hasBody?2:0)+(hint.empty()?0:2):0;
  int choiceBudget=std::min(choiceNeed,std::max(choiceCount?1:0,
    choiceNeed>rowsLeft-reserve?rowsLeft-reserve:rowsLeft));
  if(maxRows<10&&choiceBudget<choiceNeed&&choiceBudget<static_cast<int>(choiceCount)){
    // Too tight for sections and details: fall back to bare labels.
    std::vector<ChoiceRow> labels;
    for(auto& row:choiceRows)if(row.choice&&labelRow[*row.choice]==
        static_cast<std::size_t>(&row-&choiceRows[0]))labels.push_back(std::move(row));
    choiceRows=std::move(labels);
    for(std::size_t i=0;i<choiceRows.size();++i)labelRow[*choiceRows[i].choice]=i;
    choiceBudget=std::min(static_cast<int>(choiceRows.size()),std::max(1,rowsLeft));
  }
  rowsLeft-=choiceBudget;
  // A row of the request itself outranks the key hints on short screens.
  const bool showHint=!hint.empty()&&maxRows>=6&&
    rowsLeft>=(request.body.empty()&&request.preview.is_null()?1:2);
  if(showHint)rowsLeft-=1;
  const bool showSensitive=sensitive&&rowsLeft>=1;
  if(showSensitive)rowsLeft-=1;
  const bool titleRow=!framed&&!request.title.empty()&&rowsLeft>=1;
  if(titleRow)rowsLeft-=1;

  // Body: previews are one row per line; notices wrap.
  std::vector<std::string> bodyRows;
  bool bodyHidden=false;
  for(const auto& line:splitBody(request.body)){
    if(bounded){
      if(bodyRows.size()>=*request.bodyPreviewLines){bodyHidden=true;break;}
      bodyRows.push_back(line);
    }else for(auto& part:wrapWords(line,inner))bodyRows.push_back(std::move(part));
  }
  ftxui::Element preview;
  int previewRows=0;
  if(!request.preview.is_null()){
    preview=gadget_views::projectGadgetView(request.preview,gadgetViewStyle(roles,"ctrl+o"),inner);
    preview->ComputeRequirement();
    previewRows=preview->requirement().min_y;
  }
  const int bodyNeed=static_cast<int>(bodyRows.size());
  int bodyBudget=std::min(bodyNeed+(bodyHidden?1:0),rowsLeft);
  rowsLeft-=bodyBudget;
  const bool gapAfterBody=bodyBudget>0&&rowsLeft>=1&&(previewRows||choiceBudget);
  if(gapAfterBody)rowsLeft-=1;
  const int previewBudget=std::min(previewRows,rowsLeft);
  rowsLeft-=previewBudget;
  const bool gapAfterPreview=previewBudget>0&&rowsLeft>=1&&choiceBudget;
  if(gapAfterPreview)rowsLeft-=1;
  const bool gapBeforeHint=showHint&&rowsLeft>=1;

  ftxui::Elements lines;
  std::string title=request.title;
  if(bodyBudget==0&&!request.body.empty())title="… "+title;
  if(titleRow)lines.push_back(tone(fitText(value::encodeUtf8(title)),roles.cream,roles)|
    ftxui::bold);
  const int bodyShown=std::min(bodyBudget,bodyNeed);
  const bool truncated=bodyShown<bodyNeed||bodyHidden;
  for(int i=0;i<bodyShown;++i){
    auto text=bodyRows[static_cast<std::size_t>(i)];
    // The last visible row says there is more when there is no room for a note.
    if(truncated&&i+1==bodyShown&&bodyBudget==bodyShown)
      text=truncateColumns(text+" …",inner);
    lines.push_back(tone(fitText(value::encodeUtf8(text)),roles.text,roles));
  }
  if(truncated&&bodyBudget>bodyShown)
    lines.push_back(tone(fitText("… more details"),roles.muted,roles,true));
  if(gapAfterBody)lines.push_back(ftxui::text(""));
  if(previewBudget>0)
    lines.push_back(preview|ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,previewBudget));
  if(gapAfterPreview)lines.push_back(ftxui::text(""));
  if(choiceBudget>0){
    const int total=static_cast<int>(choiceRows.size());
    const int target=static_cast<int>(labelRow[selected]);
    int first=std::clamp(target-choiceBudget/2,0,std::max(0,total-choiceBudget));
    const int last=std::min(total,first+choiceBudget);
    for(int i=first;i<last;++i){
      const bool topMore=i==first&&first>0&&choiceBudget>=3&&i!=target;
      const bool bottomMore=i==last-1&&last<total&&choiceBudget>=3&&i!=target;
      if(topMore||bottomMore){
        std::size_t hidden=0;
        for(int j=topMore?0:i;j<(topMore?i+1:total);++j)
          if(choiceRows[static_cast<std::size_t>(j)].choice&&
             labelRow[*choiceRows[static_cast<std::size_t>(j)].choice]==static_cast<std::size_t>(j))++hidden;
        lines.push_back(tone(fitText(std::string(topMore?"  ↑ ":"  ↓ ")+
          std::to_string(hidden)+" more"),roles.faint,roles,true));
        continue;
      }
      lines.push_back(choiceRows[static_cast<std::size_t>(i)].element);
    }
  }
  if(showSensitive)lines.push_back(tone(fitText("Private input · hidden from poetry history"),
    roles.muted,roles,true));
  if(gapBeforeHint)lines.push_back(ftxui::text(""));
  if(showHint)lines.push_back(hintRow(hint,roles));
  auto content=ftxui::vbox(std::move(lines));
  if(!framed)return content;
  // A rounded frame with the title set into its top edge.
  const auto border=roles.borderFocus;
  const auto edge=[&](std::string text){return tone(ftxui::text(std::move(text)),border,roles,true);};
  const auto label=truncateColumns(value::encodeUtf8(title),std::max(1,columns-9));
  const int fill=std::max(0,columns-7-displayWidth(label));
  std::string dashes;
  for(int i=0;i<fill;++i)dashes+="─";
  std::string bottom="╰";
  for(int i=0;i<columns-2;++i)bottom+="─";
  bottom+="╯";
  ftxui::Elements frame;
  frame.push_back(ftxui::hbox({edge("╭─"),tone(ftxui::text(" ✦"),roles.mint,roles),
    tone(ftxui::text(" "+label+" "),roles.cream,roles)|ftxui::bold,edge(dashes+"╮")}));
  const auto side=[&]{
    auto line=ftxui::separatorStyled(ftxui::ROUNDED);
    return roles.monochrome?line|ftxui::dim:line|ftxui::color(border);
  };
  frame.push_back(ftxui::hbox({side(),ftxui::text(" "),
    content|ftxui::size(ftxui::WIDTH,ftxui::EQUAL,inner),ftxui::text(" "),side()}));
  frame.push_back(edge(bottom));
  return ftxui::vbox(std::move(frame));
}

ftxui::Element pill(const ChromeServices& imports,const ViewState& view){
  if(!view.pillVisible)return empty();
  const auto& roles=styleRoles(imports.theme.currentTheme());
  auto chip=tone(ftxui::text(" ↓ Go to the action "),roles.selection,roles)|ftxui::bold;
  if(!isDefault(roles.selectionBg))chip=chip|ftxui::bgcolor(roles.selectionBg);
  else if(roles.monochrome)chip=chip|ftxui::inverted;
  return ftxui::hbox({ftxui::filler(),std::move(chip),ftxui::filler()});
}
ftxui::Element logo(const ChromeServices& imports){
  const auto& roles=styleRoles(imports.theme.currentTheme());
  return ftxui::hbox({tone(ftxui::text("✦ "),roles.mint,roles),
    colored("Batbox",roles.brand)|ftxui::bold});
}
// Model, Roost and mode under the art, plus three tips when there is room.
std::vector<ftxui::Element> welcomeCard(const ChromeServices& imports,
    const ViewState& view,const StyleRoles& roles,bool tips){
  const int width=std::min(std::max(24,view.columns-8),64);
  const auto home=imports.homeDirectory?imports.homeDirectory():std::string{};
  auto directory=homePath(imports.workingDirectory(),home);
  const auto branch=imports.gitBranch?imports.gitBranch():std::string{};
  const auto model=imports.modelLabel();
  const auto badge=modeBadge(imports.permissionMode(),roles);
  std::vector<Segment> facts;
  if(!model.empty())facts.push_back({model,roles.cream,true});
  if(!directory.empty())facts.push_back({tailPath(directory,std::max(8,width/2)),roles.muted,false,true});
  if(!branch.empty())facts.push_back({"⎇ "+truncateColumns(branch,16),roles.muted,false,true});
  facts.push_back({badge.text,badge.color,badge.strong,!badge.strong});
  while(facts.size()>1&&segmentsWidth(facts,3)>width){
    // The Roost path shortens before any fact is dropped.
    bool shortened=false;
    for(auto& fact:facts)if(!directory.empty()&&fact.text.find('/')!=std::string::npos&&
        displayWidth(fact.text)>12){
      fact.text=tailPath(fact.text,std::max(12,displayWidth(fact.text)-
        (segmentsWidth(facts,3)-width)));
      shortened=true;break;
    }
    if(shortened&&segmentsWidth(facts,3)<=width)break;
    if(!shortened)facts.erase(facts.end()-2);
    else if(segmentsWidth(facts,3)>width)facts.erase(facts.end()-2);
  }
  std::vector<ftxui::Element> rows;
  rows.push_back(joinSegments(facts,roles," · ")|ftxui::hcenter);
  if(!tips)return rows;
  rows.push_back(ftxui::text(""));
  const std::string version=BATBOX_VERSION;
  std::vector<std::pair<std::string,std::string>> items={
    {"/help","commands"},{"@","mention a file"},{"!","run a shell command"},
    {"esc","interrupt"}};
  ftxui::Elements tipParts;
  int used=0;
  for(const auto& [key,label]:items){
    const int next=displayWidth(key)+1+displayWidth(label)+(tipParts.empty()?0:5);
    if(used+next>width)break;
    if(!tipParts.empty())tipParts.push_back(tone(ftxui::text("  ·  "),roles.faint,roles,true));
    tipParts.push_back(tone(ftxui::text(key),roles.kbd,roles)|ftxui::bold);
    tipParts.push_back(tone(ftxui::text(" "+label),roles.muted,roles,true));
    used+=next;
  }
  rows.push_back(ftxui::hbox(std::move(tipParts))|ftxui::hcenter);
  if(!version.empty())
    rows.push_back(tone(ftxui::text("v"+version),roles.faint,roles,true)|ftxui::hcenter);
  return rows;
}
ftxui::Element welcome(const ChromeServices& imports,const ViewState& view){
  const auto& roles=styleRoles(imports.theme.currentTheme());
  // The companions come from the banner art: a purple bat, a cream ghost,
  // lantern-orange stars and the mint tagline.
  const auto paint=[&](ftxui::Color color){
    return [&roles,color](std::string text){
      return roles.monochrome?ftxui::text(std::move(text)):
        ftxui::text(std::move(text))|ftxui::color(color);};
  };
  const auto purple=paint(roles.brand),cream=paint(roles.cream),
    orange=paint(roles.lantern),mint=paint(roles.mint);
  const auto frame=std::min(view.welcomeFrame,
    conversation_shell::welcomeAnimationFrames);
  const bool moving=frame<conversation_shell::welcomeAnimationFrames;
  const int available=std::max(1,view.bodyRows?view.bodyRows:view.rows);
  ftxui::Element art;
  bool large=false;
  if(view.columns<24||available<7){
    // Tiny terminals still introduce both companions without wide emoji cells.
    if(view.columns>=21)
      art=ftxui::hbox({purple("/\\^/\\"),ftxui::text("  "),
        cream("(o o)~"),ftxui::text("  "),orange("Batbox")});
    else if(view.columns>=12)
      art=ftxui::hbox({purple("/\\^/\\"),ftxui::text(" "),cream("(o o)~")});
    else if(view.columns>=9)
      art=ftxui::hbox({purple("/\\^/\\"),cream("(oo)")});
    else if(view.columns>=8)
      art=ftxui::hbox({purple("^v^"),cream("(oo)~")});
    else if(view.columns>=2)
      art=ftxui::hbox({purple("B"),cream("G")});
    else art=available>1?ftxui::vbox({purple("B"),cream("G")}):purple("B");
  }else{
    large=view.columns>=48&&available>=13;
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
        purple(line(bat,batOffset))|ftxui::size(ftxui::WIDTH,ftxui::EQUAL,batWidth),
        ftxui::text(std::string(gap,' ')),
        cream(line(ghost,ghostOffset))|ftxui::size(ftxui::WIDTH,ftxui::EQUAL,ghostWidth)}));
    }
    lines.push_back(ftxui::hbox({orange("  ✦  "),
      cream("Batbox")|ftxui::bold,orange("  ✦  ")})|ftxui::hcenter);
    if(available>=9)lines.push_back(mint("code after dark")|ftxui::hcenter);
    art=ftxui::vbox(std::move(lines));
  }
  art->ComputeRequirement();
  const int artHeight=art->requirement().min_y;
  const int rest=std::max(0,view.rows/3-view.bodyScreenRow);
  // The card joins the art only when the art can still rest at the upper third.
  std::vector<ftxui::Element> card;
  if(large){
    for(const bool tips:{true,false}){
      auto rows=welcomeCard(imports,view,roles,tips);
      const int height=static_cast<int>(rows.size())+1;
      if(rest+artHeight+height+2<=available){card=std::move(rows);break;}
    }
  }
  const int height=artHeight+(card.empty()?0:static_cast<int>(card.size())+1);
  const int bottom=height<available?1:0;
  const int top=std::clamp(rest,0,std::max(0,available-height-bottom-1));
  ftxui::Elements layout;
  if(top>0)layout.push_back(ftxui::text("")|
    ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,top));
  layout.push_back(std::move(art)|ftxui::hcenter);
  if(!card.empty()){
    layout.push_back(ftxui::text(""));
    for(auto& row:card)layout.push_back(std::move(row));
  }
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
  const auto& roles=styleRoles(imports.theme.currentTheme());
  const auto remaining=std::max(0,request.columns-displayWidth(label)-2);
  const auto left=remaining/2,right=remaining-left;
  std::string before,after;
  for(int i=0;i<left;++i)before+="─";
  for(int i=0;i<right;++i)after+="─";
  return ftxui::hbox({tone(ftxui::text(before+" "),roles.faint,roles,true),
    tone(ftxui::text(label),roles.muted,roles),
    tone(ftxui::text(" "+after),roles.faint,roles,true)});
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
  result.renderModal=[imports](const auto& view){return dialog(imports,view);};
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
  result.renderStatus=[imports](const auto& view){return statusRow(imports,view);};
  result.poetryDecor=[imports](const auto& view){return poetryDecor(imports,view);};
  result.renderSuggestions=[imports](const auto& items,std::size_t selected,
      const auto& view){return suggestionRows(items,selected,view,imports);};
  result.searchHighlight=[imports]{
    return styleRoles(imports.theme.currentTheme()).lantern;};
  return result;
}
} // namespace batbox::interactive::presentation_providers
