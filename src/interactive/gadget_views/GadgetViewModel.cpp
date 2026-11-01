#include "interactive/gadget_views/GadgetViewModel.hpp"
#include <algorithm>
#include <filesystem>

namespace batbox::interactive::gadget_views {
namespace {
std::string stringField(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
std::size_t countField(const Json& value,const char* key,std::size_t fallback=0){
  const auto& item=value::property(value,key);
  if(item.is_number_unsigned())return item.get<std::size_t>();
  if(item.is_number_integer())return static_cast<std::size_t>(std::max<long long>(0,
    item.get<long long>()));
  if(item.is_number_float())return static_cast<std::size_t>(std::max(0.0,item.get<double>()));
  return fallback;
}
bool flag(const Json& value,const char* key){
  return value::property(value,key)==true;
}
void trimTrailingBlank(std::vector<std::string>& lines){
  while(!lines.empty()&&value::trim(lines.back()).empty())lines.pop_back();
}
void trimLeadingBlank(std::vector<std::string>& lines){
  std::size_t first=0;
  while(first<lines.size()&&value::trim(lines[first]).empty())++first;
  lines.erase(lines.begin(),lines.begin()+static_cast<std::ptrdiff_t>(first));
}
std::size_t limit(bool expanded,std::size_t collapsed){
  return expanded?expandedLines:collapsed;
}
Json window(const std::vector<std::string>& lines,std::size_t count){
  Json result=Json::array();
  for(std::size_t i=0;i<lines.size()&&i<count;++i)result.push_back(lines[i]);
  return result;
}
Json base(const char* kind,bool expanded){
  return Json{{viewKey,kind},{"expanded",expanded}};
}
std::string pathLabel(std::string_view path){
  return path.empty()?std::string("file"):std::string(path);
}
} // namespace

bool isView(const Json& value){
  return value.is_object()&&value.contains(viewKey)&&value.at(viewKey).is_string();
}
std::string viewKind(const Json& value){
  return isView(value)?value.at(viewKey).get<std::string>():std::string{};
}

std::string sanitize(std::string_view text){
  std::string out;
  out.reserve(text.size());
  std::size_t lineStart=0,column=0;
  for(std::size_t i=0;i<text.size();){
    const auto ch=static_cast<unsigned char>(text[i]);
    if(ch==0x1b){
      if(i+1>=text.size()){++i;continue;}
      const char kind=text[i+1];
      if(kind=='['){
        i+=2;
        while(i<text.size()){
          const auto final=static_cast<unsigned char>(text[i++]);
          if(final>=0x40&&final<=0x7e)break;
        }
      }else if(kind==']'||kind=='P'||kind=='_'||kind=='^'||kind=='X'){
        i+=2;
        while(i<text.size()){
          if(text[i]=='\x07'){++i;break;}
          if(text[i]=='\x1b'&&i+1<text.size()&&text[i+1]=='\\'){i+=2;break;}
          ++i;
        }
      }else i+=2;
      continue;
    }
    if(ch=='\r'){
      if(i+1<text.size()&&text[i+1]=='\n'){++i;continue;}
      // A bare carriage return redraws the line: progress bars keep only
      // their final state.
      out.resize(lineStart);column=0;++i;continue;
    }
    if(ch=='\n'){out+='\n';lineStart=out.size();column=0;++i;continue;}
    if(ch=='\t'){
      const auto spaces=8-column%8;
      out.append(spaces,' ');column+=spaces;++i;continue;
    }
    if(ch<0x20||ch==0x7f){++i;continue;}
    out+=static_cast<char>(ch);
    if((ch&0xc0)!=0x80)++column;
    ++i;
  }
  return out;
}

std::vector<std::string> splitLines(std::string_view text){
  std::vector<std::string> lines;
  std::size_t start=0;
  while(start<=text.size()){
    const auto end=text.find('\n',start);
    if(end==std::string_view::npos){
      if(start<text.size())lines.emplace_back(text.substr(start));
      break;
    }
    lines.emplace_back(text.substr(start,end-start));
    start=end+1;
  }
  return lines;
}

Json textView(std::string_view text,std::string tone,bool expanded){
  auto lines=splitLines(sanitize(text));
  trimTrailingBlank(lines);
  auto view=base("text",expanded);
  view["tone"]=std::move(tone);
  view["total"]=lines.size();
  view["lines"]=window(lines,limit(expanded,collapsedLines));
  return view;
}

Json errorView(std::string_view text,bool expanded){
  auto lines=splitLines(sanitize(text));
  trimLeadingBlank(lines);trimTrailingBlank(lines);
  // Gadget errors arrive wrapped in a protocol tag; the tag is not the message.
  if(!lines.empty()&&lines.front().starts_with("<tool_use_error>")){
    lines.front().erase(0,16);
    auto& last=lines.back();
    if(last.ends_with("</tool_use_error>"))last.resize(last.size()-17);
    trimLeadingBlank(lines);trimTrailingBlank(lines);
  }
  auto view=base("error",expanded);
  view["total"]=lines.size();
  view["lines"]=window(lines,limit(expanded,collapsedLines/2));
  return view;
}

Json shellView(const Json& output,bool expanded){
  auto view=base("shell",expanded);
  if(output.is_string()){
    auto lines=splitLines(sanitize(output.get<std::string>()));
    trimTrailingBlank(lines);
    view["total"]=lines.size();
    view["lines"]=window(lines,limit(expanded,collapsedLines));
    view["errLines"]=Json::array();view["errTotal"]=0;
    return view;
  }
  auto out=splitLines(sanitize(stringField(output,"stdout")));
  auto err=splitLines(sanitize(stringField(output,"stderr")));
  trimLeadingBlank(out);trimTrailingBlank(out);
  trimLeadingBlank(err);trimTrailingBlank(err);
  const auto budget=limit(expanded,collapsedLines);
  // Standard error keeps a few rows even when standard output is long.
  const auto errShown=std::min(err.size(),err.empty()?std::size_t{0}:
    std::max<std::size_t>(budget>out.size()?budget-out.size():0,
      std::min<std::size_t>(4,budget/3)));
  const auto outShown=std::min(out.size(),budget>errShown?budget-errShown:0);
  view["lines"]=window(out,outShown);
  view["total"]=out.size();
  view["errLines"]=window(err,errShown);
  view["errTotal"]=err.size();
  view["interrupted"]=flag(output,"interrupted");
  const auto note=stringField(output,"returnCodeInterpretation");
  if(!note.empty())view["note"]=note;
  const auto background=stringField(output,"backgroundTaskId");
  if(!background.empty())view["background"]=background;
  if(flag(output,"isImage"))view["note"]="Image output";
  return view;
}

Json readView(const Json& output,bool expanded){
  const auto type=stringField(output,"type");
  const auto& file=value::property(output,"file");
  auto view=base("lines",expanded);
  view["lines"]=Json::array();view["total"]=0;
  if(type=="file_unchanged"){
    view["summary"]="Unchanged since the last read";view["tone"]="muted";
    return view;
  }
  if(type=="image"){
    view["summary"]="Read image";
    const auto& dimensions=value::property(file,"dimensions");
    const auto width=countField(dimensions,"originalWidth"),
      height=countField(dimensions,"originalHeight");
    if(width&&height)view["summary"]="Read image ("+std::to_string(width)+"×"+
      std::to_string(height)+")";
    return view;
  }
  if(type=="notebook"){
    const auto& cells=value::property(file,"cells");
    const auto count=cells.is_array()?cells.size():0;
    view["summary"]="Read notebook ("+plural(count,"cell","cells")+")";
    return view;
  }
  if(type=="pdf"||type=="parts"){
    view["summary"]="Read PDF";
    return view;
  }
  const auto content=sanitize(stringField(file,"content"));
  auto lines=splitLines(content);
  const auto count=countField(file,"numLines",lines.size());
  const auto start=std::max<std::size_t>(1,countField(file,"startLine",1));
  const auto total=countField(file,"totalLines",count);
  std::string summary="Read "+plural(count,"line","lines");
  if(count==0)summary=total==0?"Read an empty file":"No lines in that range";
  else if(start>1||count<total)
    summary+=" ("+std::to_string(start)+"–"+std::to_string(start+count-1)+
      " of "+std::to_string(total)+")";
  view["summary"]=summary;
  view["start"]=start;
  view["total"]=lines.size();
  view["lines"]=window(lines,limit(expanded,excerptLines));
  return view;
}

Json listView(std::string summary,const std::vector<std::string>& items,
    std::size_t total,bool expanded){
  auto view=base("list",expanded);
  view["summary"]=std::move(summary);
  std::vector<std::string> clean;
  clean.reserve(std::min(items.size(),limit(expanded,listItems)));
  for(std::size_t i=0;i<items.size()&&i<limit(expanded,listItems);++i)
    clean.push_back(sanitize(items[i]));
  view["items"]=clean;
  view["total"]=std::max(total,items.size());
  return view;
}

Json globView(const Json& output,bool expanded){
  std::vector<std::string> files;
  const auto& names=value::property(output,"filenames");
  if(names.is_array())for(const auto& name:names)
    if(name.is_string())files.push_back(name.get<std::string>());
  const auto count=countField(output,"numFiles",files.size());
  std::string summary=count==0?"No files found":"Found "+plural(count,"file","files");
  if(flag(output,"truncated"))summary+=" (truncated)";
  return listView(std::move(summary),files,count,expanded);
}

Json grepView(const Json& output,bool expanded){
  const auto mode=stringField(output,"mode");
  if(mode=="content"||mode=="count"){
    auto lines=splitLines(sanitize(stringField(output,"content")));
    trimTrailingBlank(lines);
    std::string summary;
    if(mode=="count"){
      const auto matches=countField(output,"numMatches");
      const auto files=countField(output,"numFiles");
      summary="Found "+plural(matches,"match","matches")+" across "+
        plural(files,"file","files");
    }else{
      const auto count=countField(output,"numLines",lines.size());
      summary=count==0?"No matches":"Found "+plural(count,"line","lines");
    }
    return listView(std::move(summary),lines,lines.size(),expanded);
  }
  return globView(output,expanded);
}

Json diffView(std::string_view verb,std::string_view path,
    const Json& structuredPatch,bool expanded){
  auto view=base("diff",expanded);
  std::size_t added=0,removed=0,total=0;
  if(structuredPatch.is_array())for(const auto& hunk:structuredPatch){
    const auto& lines=value::property(hunk,"lines");
    if(!lines.is_array())continue;
    for(const auto& line:lines){
      if(!line.is_string())continue;
      const auto& text=line.get_ref<const std::string&>();
      if(text.starts_with("+"))++added;
      else if(text.starts_with("-"))++removed;
      ++total;
    }
  }
  if(added==0&&removed==0){
    view["summary"]="No changes to "+pathLabel(path);
  }else{
    std::string summary=std::string(verb)+" "+pathLabel(path)+" with ";
    if(added)summary+=plural(added,"addition","additions");
    if(added&&removed)summary+=" and ";
    if(removed)summary+=plural(removed,"removal","removals");
    view["summary"]=summary;
  }
  view["added"]=added;view["removed"]=removed;view["total"]=total;
  // Hunks keep their own line numbers; only the visible rows are copied.
  const auto budget=limit(expanded,24);
  Json hunks=Json::array();
  std::size_t shown=0;
  if(structuredPatch.is_array())for(const auto& hunk:structuredPatch){
    if(shown>=budget)break;
    const auto& lines=value::property(hunk,"lines");
    if(!lines.is_array())continue;
    Json copy={{"oldStart",countField(hunk,"oldStart",1)},
      {"newStart",countField(hunk,"newStart",1)},{"lines",Json::array()}};
    for(const auto& line:lines){
      if(shown>=budget)break;
      if(!line.is_string())continue;
      copy["lines"].push_back(sanitize(line.get<std::string>()));
      ++shown;
    }
    hunks.push_back(std::move(copy));
  }
  view["hunks"]=std::move(hunks);
  view["shown"]=shown;
  return view;
}

Json createdFileView(std::string_view path,std::string_view content,bool expanded){
  auto lines=splitLines(sanitize(content));
  auto view=base("diff",expanded);
  view["summary"]="Created "+pathLabel(path)+" ("+plural(lines.size(),"line","lines")+")";
  view["added"]=lines.size();view["removed"]=0;view["total"]=lines.size();
  Json body=Json::array();
  const auto budget=limit(expanded,excerptLines);
  for(std::size_t i=0;i<lines.size()&&i<budget;++i)body.push_back("+"+lines[i]);
  view["shown"]=body.size();
  view["hunks"]=Json::array({Json{{"oldStart",0},{"newStart",1},{"lines",std::move(body)}}});
  return view;
}

Json editPreview(std::string_view path,std::string_view before,std::string_view after,
    std::size_t maxLines){
  const auto old=splitLines(sanitize(before)),fresh=splitLines(sanitize(after));
  std::size_t prefix=0;
  while(prefix<std::min(old.size(),fresh.size())&&old[prefix]==fresh[prefix])++prefix;
  std::size_t suffix=0;
  while(suffix<old.size()-prefix&&suffix<fresh.size()-prefix&&
      old[old.size()-1-suffix]==fresh[fresh.size()-1-suffix])++suffix;
  std::vector<std::string> body;
  const auto contextBefore=std::min<std::size_t>(2,prefix);
  for(auto i=prefix-contextBefore;i<prefix;++i)body.push_back(" "+old[i]);
  for(auto i=prefix;i<old.size()-suffix;++i)body.push_back("-"+old[i]);
  for(auto i=prefix;i<fresh.size()-suffix;++i)body.push_back("+"+fresh[i]);
  for(std::size_t i=old.size()-suffix;i<old.size()&&i<old.size()-suffix+2;++i)
    body.push_back(" "+old[i]);
  auto view=base("diff",false);
  std::size_t added=0,removed=0;
  for(const auto& line:body){if(line.starts_with("+"))++added;else if(line.starts_with("-"))++removed;}
  // The dialog body already names the file; the preview is the change itself.
  (void)path;
  view["summary"]="";
  view["added"]=added;view["removed"]=removed;view["total"]=body.size();
  view["numbers"]=false;
  Json lines=Json::array();
  for(std::size_t i=0;i<body.size()&&i<maxLines;++i)lines.push_back(body[i]);
  view["shown"]=lines.size();
  view["hunks"]=Json::array({Json{{"oldStart",0},{"newStart",0},{"lines",std::move(lines)}}});
  return view;
}

Json writePreview(std::string_view path,std::string_view content,std::size_t maxLines){
  auto view=createdFileView(path,content,false);
  auto& lines=view["hunks"][0]["lines"];
  const auto all=splitLines(sanitize(content));
  lines=Json::array();
  for(std::size_t i=0;i<all.size()&&i<maxLines;++i)lines.push_back("+"+all[i]);
  view["shown"]=lines.size();
  view["summary"]="";
  view["numbers"]=true;
  return view;
}

std::string displayPath(std::string_view path,std::string_view cwd,
    std::string_view home){
  if(path.empty())return {};
  namespace fs=std::filesystem;
  const fs::path target=fs::path(std::string(path)).lexically_normal();
  if(!target.is_absolute())return target.generic_string();
  const auto under=[&](std::string_view root)->std::optional<std::string>{
    if(root.empty())return {};
    const auto relative=target.lexically_relative(fs::path(std::string(root)).lexically_normal());
    if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")return {};
    return relative.generic_string();
  };
  if(auto relative=under(cwd))return *relative=="."?std::string("."):*relative;
  // The Roost may be recorded through a symlink (/var and /private/var on
  // macOS); compare resolved paths before giving up.
  std::error_code error;
  const auto resolved=fs::weakly_canonical(target,error);
  if(!error&&!cwd.empty()){
    const auto root=fs::weakly_canonical(fs::path(std::string(cwd)),error);
    if(!error){
      const auto relative=resolved.lexically_relative(root);
      if(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..")
        return relative=="."?std::string("."):relative.generic_string();
    }
  }
  if(auto relative=under(home))return *relative=="."?std::string("~"):"~/"+*relative;
  return target.generic_string();
}

std::string plural(std::size_t count,std::string_view one,std::string_view many){
  return std::to_string(count)+" "+std::string(count==1?one:many);
}
} // namespace batbox::interactive::gadget_views
