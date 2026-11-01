#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "terminal_ftxui/TextFit.hpp"
#include <ftxui/screen/string.hpp>
#include <algorithm>
#include <functional>
#include <utility>

namespace batbox::interactive::gadget_views {
namespace {
using terminal_ftxui::fitText;

std::string string(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
std::size_t count(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  if(item.is_number_unsigned())return item.get<std::size_t>();
  if(item.is_number_integer())return static_cast<std::size_t>(std::max<long long>(0,item.get<long long>()));
  return 0;
}
std::vector<std::string> strings(const Json& value,const char* key){
  std::vector<std::string> result;
  const auto& items=value::property(value,key);
  if(items.is_array())for(const auto& item:items)
    if(item.is_string())result.push_back(item.get<std::string>());
  return result;
}
ftxui::Element tint(ftxui::Element element,ftxui::Color color,
    const GadgetViewStyle& style,bool quiet=false){
  if(style.monochrome)return quiet?element|ftxui::dim:element;
  return element|ftxui::color(color);
}
ftxui::Element quiet(ftxui::Element element,const GadgetViewStyle& style){
  return tint(std::move(element),style.muted,style,true);
}
std::vector<std::string> hardWrap(const std::string& line,int width){
  if(width<=0||terminal_ftxui::displayWidth(line)<=width)return {line};
  std::vector<std::string> rows(1);
  int used=0;
  for(const auto& glyph:ftxui::Utf8ToGlyphs(line)){
    if(glyph.empty())continue;
    const int next=ftxui::string_width(glyph);
    if(used+next>width){rows.emplace_back();used=0;}
    rows.back()+=glyph;used+=next;
  }
  return rows;
}
// Collapsed output keeps one row per line; the transcript wraps everything.
void appendLine(ftxui::Elements& rows,const std::string& line,bool expanded,int width,
    const std::function<ftxui::Element(ftxui::Element)>& decorate){
  if(!expanded){rows.push_back(decorate(fitText(line)));return;}
  for(auto& part:hardWrap(line,width))rows.push_back(decorate(ftxui::text(part)));
}
ftxui::Element moreHint(std::size_t hidden,bool expanded,const GadgetViewStyle& style){
  std::string label="… +"+plural(hidden,"line","lines");
  if(!expanded)label+=" ("+style.expandKey+" to expand)";
  return tint(fitText(std::move(label)),style.faint,style,true);
}
ftxui::Element summaryRow(const std::string& summary,const std::string& tone,
    const GadgetViewStyle& style){
  auto row=fitText(summary);
  if(tone=="muted")return quiet(std::move(row),style);
  if(tone=="error")return tint(std::move(row),style.error,style);
  return tint(std::move(row),style.text,style);
}
ftxui::Color toneColor(const std::string& tone,const GadgetViewStyle& style){
  if(tone=="muted")return style.muted;
  if(tone=="error")return style.error;
  if(tone=="ok")return style.ok;
  return style.text;
}

ftxui::Element textBody(const Json& view,const GadgetViewStyle& style,int width,
    ftxui::Color color,bool quietRows){
  const bool expanded=value::property(view,"expanded")==true;
  const auto lines=strings(view,"lines");
  ftxui::Elements rows;
  for(const auto& line:lines)
    appendLine(rows,line,expanded,width,[&](ftxui::Element element){
      return tint(std::move(element),color,style,quietRows);});
  const auto total=count(view,"total");
  if(total>lines.size())rows.push_back(moreHint(total-lines.size(),expanded,style));
  return ftxui::vbox(std::move(rows));
}

ftxui::Element linesBody(const Json& view,const GadgetViewStyle& style,int width){
  const bool expanded=value::property(view,"expanded")==true;
  ftxui::Elements rows;
  rows.push_back(summaryRow(string(view,"summary"),string(view,"tone"),style));
  const auto lines=strings(view,"lines");
  const auto start=std::max<std::size_t>(1,count(view,"start"));
  const auto digits=static_cast<int>(std::to_string(start+lines.size()).size());
  for(std::size_t i=0;i<lines.size();++i){
    auto number=std::to_string(start+i);
    number.insert(0,static_cast<std::size_t>(std::max(0,digits-static_cast<int>(number.size()))),' ');
    rows.push_back(ftxui::hbox({
      tint(ftxui::text(number+"  "),style.diffLineNo,style,true),
      quiet(expanded?ftxui::text(lines[i]):fitText(lines[i]),style)|ftxui::xflex}));
  }
  (void)width;
  const auto total=count(view,"total");
  if(total>lines.size())rows.push_back(moreHint(total-lines.size(),expanded,style));
  return ftxui::vbox(std::move(rows));
}

ftxui::Element listBody(const Json& view,const GadgetViewStyle& style,int width){
  const bool expanded=value::property(view,"expanded")==true;
  ftxui::Elements rows;
  rows.push_back(summaryRow(string(view,"summary"),string(view,"tone"),style));
  const auto items=strings(view,"items");
  for(const auto& item:items)
    appendLine(rows,item,expanded,width,[&](ftxui::Element element){
      return quiet(std::move(element),style);});
  const auto total=count(view,"total");
  if(total>items.size())rows.push_back(moreHint(total-items.size(),expanded,style));
  return ftxui::vbox(std::move(rows));
}

ftxui::Element shellBody(const Json& view,const GadgetViewStyle& style,int width){
  const bool expanded=value::property(view,"expanded")==true;
  ftxui::Elements rows;
  const auto out=strings(view,"lines"),err=strings(view,"errLines");
  for(const auto& line:out)
    appendLine(rows,line,expanded,width,[&](ftxui::Element element){
      return tint(std::move(element),style.text,style);});
  for(const auto& line:err)
    appendLine(rows,line,expanded,width,[&](ftxui::Element element){
      return tint(std::move(element),style.error,style);});
  const auto hidden=(count(view,"total")-std::min(count(view,"total"),out.size()))+
    (count(view,"errTotal")-std::min(count(view,"errTotal"),err.size()));
  if(hidden)rows.push_back(moreHint(hidden,expanded,style));
  if(value::property(view,"interrupted")==true)
    rows.push_back(tint(fitText("Interrupted"),style.warn,style));
  const auto background=string(view,"background");
  if(!background.empty())
    rows.push_back(quiet(fitText("Running in the background · "+background),style));
  const auto note=string(view,"note");
  if(!note.empty())rows.push_back(quiet(fitText(note),style));
  if(rows.empty())rows.push_back(quiet(ftxui::text("(No output)"),style));
  return ftxui::vbox(std::move(rows));
}

ftxui::Element diffBody(const Json& view,const GadgetViewStyle& style,int width){
  const bool expanded=value::property(view,"expanded")==true;
  const bool numbers=value::property(view,"numbers")!=false;
  ftxui::Elements rows;
  const auto summary=string(view,"summary");
  if(!summary.empty())rows.push_back(summaryRow(summary,{},style));
  const auto& hunks=value::property(view,"hunks");
  std::size_t widest=1;
  if(numbers&&hunks.is_array())for(const auto& hunk:hunks){
    const auto size=strings(hunk,"lines").size();
    widest=std::max({widest,count(hunk,"oldStart")+size,count(hunk,"newStart")+size});
  }
  const auto digits=std::to_string(widest).size();
  bool first=true;
  if(hunks.is_array())for(const auto& hunk:hunks){
    if(!first)rows.push_back(tint(ftxui::text(std::string(numbers?digits:0,' ')+" ⋮"),
      style.diffHunk,style,true));
    first=false;
    auto oldLine=count(hunk,"oldStart"),newLine=count(hunk,"newStart");
    for(const auto& raw:strings(hunk,"lines")){
      const char sign=raw.empty()?' ':raw.front();
      const auto body=raw.empty()?std::string{}:raw.substr(1);
      std::size_t number=0;
      if(sign=='+')number=newLine++;
      else if(sign=='-')number=oldLine++;
      else{number=newLine;++oldLine;++newLine;}
      ftxui::Elements cells;
      if(numbers){
        auto label=number?std::to_string(number):std::string{};
        label.insert(0,digits>label.size()?digits-label.size():0,' ');
        cells.push_back(tint(ftxui::text(label+" "),style.diffLineNo,style,true));
      }
      const auto marker=sign=='+'?"+ ":sign=='-'?"- ":"  ";
      auto content=expanded?ftxui::text(std::string(marker)+body):
        fitText(std::string(marker)+body);
      if(sign=='+')content=tint(std::move(content),style.diffAdd,style);
      else if(sign=='-')content=tint(std::move(content),style.diffDel,style);
      else content=tint(std::move(content),style.diffContext,style,true);
      if(style.monochrome&&sign!=' ')content=content|ftxui::bold;
      cells.push_back(std::move(content)|ftxui::xflex);
      auto row=ftxui::hbox(std::move(cells));
      const auto background=sign=='+'?style.diffAddBg:sign=='-'?style.diffDelBg:
        ftxui::Color::Default;
      if(!style.monochrome&&!(background==ftxui::Color::Default))
        row=row|ftxui::bgcolor(background);
      rows.push_back(std::move(row));
    }
  }
  (void)width;
  const auto total=count(view,"total"),shown=count(view,"shown");
  if(total>shown)rows.push_back(moreHint(total-shown,expanded,style));
  return ftxui::vbox(std::move(rows));
}
} // namespace

ftxui::Element gadgetResponse(ftxui::Element body,const GadgetViewStyle& style){
  return ftxui::hbox({
    tint(ftxui::text("  ⎿  "),style.faint,style,true),
    std::move(body)|ftxui::xflex});
}

ftxui::Element projectGadgetView(const Json& view,const GadgetViewStyle& style,
    int columns){
  const auto kind=viewKind(view);
  const int width=std::max(1,columns);
  if(kind=="diff")return diffBody(view,style,width);
  if(kind=="shell")return shellBody(view,style,width);
  if(kind=="lines")return linesBody(view,style,width);
  if(kind=="list")return listBody(view,style,width);
  if(kind=="error")return textBody(view,style,width,style.error,false);
  const auto tone=string(view,"tone");
  return textBody(view,style,width,toneColor(tone,style),tone=="muted");
}

ftxui::Element projectGadgetResult(const Json& value,const GadgetViewStyle& style,
    int columns){
  const int width=std::max(1,columns-resultIndent);
  if(isView(value))return gadgetResponse(projectGadgetView(value,style,width),style);
  const auto text=visibleText(value);
  if(value::trim(text).empty())return ftxui::vbox(ftxui::Elements{});
  return gadgetResponse(projectGadgetView(textView(text,"normal",false),style,width),style);
}

ftxui::Element projectMissionNotice(const MissionNotice& notice,
    const GadgetViewStyle& style){
  const bool failed=notice.outcome=="failed";
  const bool finished=notice.outcome=="finished";
  auto glyph=ftxui::text(failed?"✗ ":finished?"✓ ":"■ ")|ftxui::bold;
  glyph=failed?tint(std::move(glyph),style.error,style):
    finished?tint(std::move(glyph),style.ok,style):quiet(std::move(glyph),style);
  const auto who=sanitize(notice.kind+(notice.name.empty()?"":" "+notice.name));
  ftxui::Elements cells{std::move(glyph),
    tint(ftxui::text(who),style.text,style)|ftxui::bold,
    tint(ftxui::text(" "+notice.outcome),failed?style.error:style.text,style)};
  if(!notice.detail.empty())
    cells.push_back(quiet(fitText(" · "+sanitize(notice.detail)),style)|ftxui::xflex);
  return ftxui::hbox(std::move(cells));
}

ftxui::Element projectGadgetInline(const Json& value,ftxui::Color color){
  auto text=sanitize(visibleText(value));
  const auto end=text.find('\n');
  if(end!=std::string::npos)text=text.substr(0,end)+" …";
  auto element=fitText(std::move(text));
  return color==ftxui::Color::Default?element:element|ftxui::color(color);
}

} // namespace batbox::interactive::gadget_views
