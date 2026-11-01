#include "flights/FlightListing.hpp"
#include "flights/DisplayTitle.hpp"
#include "flights/IndigenousPath.hpp"
#include "flights/Views.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace batbox::flights {
namespace {
constexpr std::size_t tailBytes=65536;
constexpr std::size_t headChunkBytes=16384;
constexpr std::size_t headLimitBytes=262144;
constexpr std::size_t titleColumns=60;

std::string stringField(const Json& value,const char* key){
  const auto& field=value::property(value,key);
  return field.is_string()?field.get<std::string>():std::string{};
}
std::string folded(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  return value;
}
std::string oneLine(std::string value){
  for(auto& c:value)if(c=='\n'||c=='\r'||c=='\t')c=' ';
  std::string collapsed;collapsed.reserve(value.size());
  for(const char c:value){
    if(c==' '&&!collapsed.empty()&&collapsed.back()==' ')continue;
    collapsed.push_back(c);
  }
  return value::trim(collapsed);
}
std::string excerpt(const std::string& value,std::size_t limit){
  auto line=oneLine(value);
  if(value::stringLength(line)<=limit)return line;
  return value::trim(value::slice(line,0,static_cast<double>(limit-1)))+"…";
}
std::optional<Json> parseLine(std::string_view line){
  while(!line.empty()&&(line.back()=='\r'||line.back()==' '))line.remove_suffix(1);
  if(line.empty())return std::nullopt;
  try{
    auto parsed=value::parse(value::decodeUtf8(line));
    if(parsed.is_object())return parsed;
  }catch(const std::exception&){}
  return std::nullopt;
}
// Parse complete lines of text. A leading fragment (tail reads start mid-line)
// and a trailing fragment (a write cut short) are both skipped.
void parseLines(std::string_view text,bool skipFirst,std::vector<Json>& out){
  std::size_t start=0;
  if(skipFirst){
    const auto first=text.find('\n');
    if(first==std::string_view::npos)return;
    start=first+1;
  }
  while(start<text.size()){
    const auto end=text.find('\n',start);
    if(end==std::string_view::npos){
      // A final line without a newline is still complete if it parses.
      if(auto parsed=parseLine(text.substr(start)))out.push_back(std::move(*parsed));
      break;
    }
    if(auto parsed=parseLine(text.substr(start,end-start)))out.push_back(std::move(*parsed));
    start=end+1;
  }
}
bool isChain(const Json& entry){
  const auto& type=value::property(entry,"type");
  return (type=="user"||type=="assistant")&&
    !value::truthy(value::property(entry,"isSidechain"));
}
const Json& content(const Json& entry){
  return value::property(value::property(entry,"message"),"content");
}
std::string firstText(const Json& entry){
  const auto& body=content(entry);
  if(body.is_string())return body.get<std::string>();
  if(body.is_array())for(const auto& block:body)
    if(value::property(block,"type")=="text"){
      const auto text=stringField(block,"text");
      if(!value::trim(text).empty())return text;
    }
  return {};
}
bool hasBlock(const Json& entry,const char* type){
  const auto& body=content(entry);
  if(!body.is_array())return false;
  for(const auto& block:body)if(value::property(block,"type")==type)return true;
  return false;
}
// Local command echoes and meta caveats are written after a reply has already
// finished, so they say nothing about whether the Flight was interrupted.
bool bookkeeping(const Json& entry){
  if(value::property(entry,"type")!="user")return false;
  if(value::truthy(value::property(entry,"isMeta")))return true;
  const auto text=value::trim(firstText(entry));
  return text.starts_with("<local-command")||text.starts_with("<command-name>")||
    text.starts_with("<command-message>");
}
std::string endStateOf(const Json& entry){
  if(value::property(entry,"type")=="assistant"){
    if(value::truthy(value::property(entry,"isApiErrorMessage")))return "error";
    if(hasBlock(entry,"tool_use"))return "interrupted";
    return hasBlock(entry,"text")?"":"interrupted";
  }
  // The newest significant entry is a user turn: a Gadget result or prompt
  // that never received a reply, or an explicit interruption marker.
  return "interrupted";
}
void readHead(std::ifstream& input,std::size_t limit,
    std::vector<Json>& entries,std::size_t& consumed,
    const std::function<bool(const std::vector<Json>&)>& satisfied){
  std::string pending;
  consumed=0;
  while(consumed<limit){
    std::string chunk(std::min(headChunkBytes,limit-consumed),'\0');
    input.read(chunk.data(),static_cast<std::streamsize>(chunk.size()));
    const auto count=static_cast<std::size_t>(input.gcount());
    if(count==0)break;
    chunk.resize(count);consumed+=count;
    pending+=chunk;
    std::size_t start=0;
    while(true){
      const auto end=pending.find('\n',start);
      if(end==std::string::npos)break;
      if(auto parsed=parseLine(std::string_view(pending).substr(start,end-start)))
        entries.push_back(std::move(*parsed));
      start=end+1;
    }
    pending.erase(0,start);
    if(satisfied(entries))return;
  }
}
Json chainArray(const std::vector<Json>& entries){
  Json values=Json::array();
  for(const auto& entry:entries)if(isChain(entry))values.push_back(entry);
  return values;
}
} // namespace

std::optional<double> isoTimestampMs(const std::string& value){
  int year=0,month=0,day=0,hour=0,minute=0,second=0;
  if(std::sscanf(value.c_str(),"%d-%d-%dT%d:%d:%d",
      &year,&month,&day,&hour,&minute,&second)!=6)return std::nullopt;
  using namespace std::chrono;
  const year_month_day date{std::chrono::year(year),
    std::chrono::month(static_cast<unsigned>(month)),
    std::chrono::day(static_cast<unsigned>(day))};
  if(!date.ok()||hour<0||hour>23||minute<0||minute>59||second<0||second>60)
    return std::nullopt;
  double result=static_cast<double>(duration_cast<milliseconds>(
    sys_days(date).time_since_epoch()).count());
  result+=static_cast<double>((hour*3600+minute*60+second)*1000);
  const auto dot=value.find('.',19);
  if(dot!=std::string::npos){
    int fraction=0,multiplier=100;
    for(std::size_t i=dot+1;i<value.size()&&multiplier>0;++i){
      const char c=value[i];if(c<'0'||c>'9')break;
      fraction+=(c-'0')*multiplier;multiplier/=10;
    }
    result+=fraction;
  }
  return result;
}

std::optional<Json> scanFlightFileLite(const std::string& path,
    const std::string& flightId,double modifiedMs,PoetryServices& poetry){
  std::error_code error;
  const auto file=indigenousPath(path);
  const auto size=std::filesystem::file_size(file,error);
  if(error||size==0)return std::nullopt;
  std::ifstream input(file,std::ios::binary);
  if(!input)return std::nullopt;
  std::vector<Json> head,tail;
  std::size_t headConsumed=0,tailConsumed=0;
  const bool complete=size<=2*tailBytes;
  if(complete){
    std::string text(static_cast<std::size_t>(size),'\0');
    input.read(text.data(),static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(input.gcount()));
    if(text.starts_with("\xef\xbb\xbf"))text.erase(0,3);
    parseLines(text,false,head);
    headConsumed=text.size();
  }else{
    const auto found=[&poetry](const std::vector<Json>& entries){
      const auto chain=chainArray(entries);
      return getFirstMeaningfulUserMessageTextContent(chain,poetry).has_value();
    };
    // Stop before the tail window so the two regions never overlap.
    const auto limit=static_cast<std::size_t>(std::min<std::uintmax_t>(
      headLimitBytes,size-tailBytes));
    readHead(input,limit,head,headConsumed,found);
    input.clear();
    const auto offset=size-tailBytes;
    input.seekg(static_cast<std::streamoff>(offset),std::ios::beg);
    std::string text(tailBytes,'\0');
    input.read(text.data(),static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(input.gcount()));
    tailConsumed=text.size();
    parseLines(text,true,tail);
  }
  // Everything is in head for small files; scan both regions in file order.
  std::vector<const Json*> ordered;
  for(const auto& entry:head)ordered.push_back(&entry);
  for(const auto& entry:tail)ordered.push_back(&entry);

  Json out={{"sessionId",flightId},{"messages",Json::array()},
    {"fullPath",path},{"modifiedMs",modifiedMs},{"isSidechain",false}};
  const Json* firstChain=nullptr;const Json* lastChain=nullptr;
  std::size_t lastChainAt=0,flightEndAt=0;bool sawEnd=false;
  std::unordered_map<std::string,const Json*> byUuid;
  std::unordered_set<std::string> parents;
  std::string slug,branch,model,lastAssistant,cwd;
  for(std::size_t index=0;index<ordered.size();++index){
    const auto& entry=*ordered[index];
    const auto type=stringField(entry,"type");
    const auto owner=stringField(entry,"sessionId");
    const bool ours=owner.empty()||owner==flightId;
    if(type=="custom-title"&&ours){out["customTitle"]=stringField(entry,"customTitle");continue;}
    if(type=="ai-title"&&ours){out["aiTitle"]=stringField(entry,"aiTitle");continue;}
    if(type=="tag"&&ours){out["tag"]=stringField(entry,"tag");continue;}
    if(type=="agent-name"&&ours){out["agentName"]=stringField(entry,"agentName");continue;}
    if(type=="flight-end"&&ours){sawEnd=true;flightEndAt=index;continue;}
    if(!isChain(entry))continue;
    if(!firstChain)firstChain=&entry;
    lastChain=&entry;lastChainAt=index;
    if(const auto uuid=stringField(entry,"uuid");!uuid.empty())byUuid[uuid]=&entry;
    if(const auto parent=stringField(entry,"parentUuid");!parent.empty())parents.insert(parent);
    if(slug.empty())slug=stringField(entry,"slug");
    if(const auto value=stringField(entry,"gitBranch");!value.empty())branch=value;
    if(const auto value=stringField(entry,"cwd");!value.empty()&&cwd.empty())cwd=value;
    if(type=="assistant"){
      const auto name=stringField(value::property(entry,"message"),"model");
      if(!name.empty()&&name!="<synthetic>")model=name;
      if(hasBlock(entry,"text")&&!value::truthy(value::property(entry,"isApiErrorMessage"))){
        const auto text=firstText(entry);
        if(!value::trim(text).empty())lastAssistant=text;
      }
    }
  }
  // The write queue does not keep file order equal to chain order (a prompt
  // can land after its reply), so the conversation's end is the newest leaf
  // of the parentUuid graph, walked back past local command bookkeeping.
  const auto stampOf=[](const Json& entry){
    const auto& stamp=value::property(entry,"timestamp");
    const auto parsed=stamp.is_string()?isoTimestampMs(stamp.get<std::string>()):std::nullopt;
    return parsed.value_or(-1.0);
  };
  const Json* leaf=nullptr;double leafStamp=-2.0;double newest=-1.0;
  for(const auto* item:ordered){
    if(!isChain(*item))continue;
    const auto stamp=stampOf(*item);
    newest=std::max(newest,stamp);
    const auto uuid=stringField(*item,"uuid");
    if(!uuid.empty()&&parents.contains(uuid))continue;
    if(stamp>=leafStamp){leaf=item;leafStamp=stamp;}
  }
  if(!leaf)leaf=lastChain;
  const Json* lastSignificant=leaf;
  for(std::size_t guard=0;lastSignificant&&bookkeeping(*lastSignificant)&&
      guard<byUuid.size();++guard){
    const auto parent=stringField(*lastSignificant,"parentUuid");
    const auto found=byUuid.find(parent);
    lastSignificant=found==byUuid.end()?nullptr:found->second;
  }
  std::optional<std::string> prompt=getFirstMeaningfulUserMessageTextContent(
    chainArray(head),poetry);
  if(!prompt)prompt=getFirstMeaningfulUserMessageTextContent(chainArray(tail),poetry);
  if(!prompt||value::trim(*prompt).empty())return std::nullopt;
  out["firstPrompt"]=excerpt(*prompt,200);
  if(firstChain){
    out["created"]=value::property(*firstChain,"timestamp");
    const auto firstCwd=stringField(*firstChain,"cwd");
    out["projectPath"]=firstCwd.empty()?cwd:firstCwd;
  }
  if(leaf){
    out["modified"]=value::property(*leaf,"timestamp");
    out["date"]=value::property(*leaf,"timestamp");
  }
  out["lastActivityMs"]=newest>=0?newest:modifiedMs;
  if(!slug.empty())out["slug"]=slug;
  if(!branch.empty())out["gitBranch"]=branch;
  if(!model.empty())out["model"]=model;
  if(!lastAssistant.empty())out["lastAssistantText"]=excerpt(lastAssistant,200);
  std::string state=lastSignificant?endStateOf(*lastSignificant):"";
  // flight-end follows a full flush, so a chain line after it belongs to a
  // later, resumed session.
  if(state.empty()&&sawEnd&&flightEndAt>lastChainAt)state="ended";
  out["endState"]=state;
  Json sample=chainArray(head);
  for(const auto& entry:tail)if(isChain(entry))sample.push_back(entry);
  const auto visible=countVisibleMessages(sample);
  if(complete){
    out["messageCount"]=visible;out["messageCountApproximate"]=false;
  }else{
    const double sampled=static_cast<double>(headConsumed+tailConsumed);
    const double estimate=sampled>0?std::round(static_cast<double>(visible)*
      static_cast<double>(size)/sampled):static_cast<double>(visible);
    out["messageCount"]=static_cast<std::size_t>(std::max(estimate,
      static_cast<double>(visible)));
    out["messageCountApproximate"]=true;
  }
  return out;
}

std::string relativeTimeText(double thenMs,double nowMs){
  const double seconds=std::max(0.0,(nowMs-thenMs)/1000.0);
  const auto unit=[](long long count,const char* singular,const char* plural){
    return std::to_string(count)+" "+(count==1?singular:plural)+" ago";
  };
  if(seconds<45)return "just now";
  const auto minutes=static_cast<long long>(std::llround(seconds/60));
  if(minutes<60)return unit(minutes,"minute","minutes");
  const auto hours=static_cast<long long>(std::llround(seconds/3600));
  if(hours<24)return unit(hours,"hour","hours");
  const auto days=static_cast<long long>(std::llround(seconds/86400));
  if(days<=1)return "yesterday";
  if(days<7)return unit(days,"day","days");
  if(days<30)return unit(std::max(1LL,std::llround(days/7.0)),"week","weeks");
  if(days<365)return unit(std::max(1LL,std::llround(days/30.0)),"month","months");
  return unit(std::max(1LL,std::llround(days/365.0)),"year","years");
}

std::string shortModelName(const std::string& model){
  auto name=model;
  if(const auto slash=name.rfind('/');slash!=std::string::npos)name.erase(0,slash+1);
  if(name.starts_with("claude-"))name.erase(0,7);
  const auto dated=[&name](char separator){
    if(name.size()<10)return false;
    const auto at=name.size()-9;
    if(name[at]!=separator)return false;
    for(std::size_t i=at+1;i<name.size();++i)
      if(name[i]<'0'||name[i]>'9')return false;
    return true;
  };
  if(dated('-')||dated('@'))name.erase(name.size()-9);
  if(value::stringLength(name)>24)name=value::slice(name,0,23)+"…";
  return name;
}

std::string flightListingTitle(const Json& log){
  auto title=oneLine(getLogDisplayTitle(log,std::nullopt));
  if(title.empty()){
    title=stringField(log,"slug");
    if(title.empty()){
      const auto id=stringField(log,"sessionId");
      title=id.size()>8?id.substr(0,8):id;
    }
  }
  return excerpt(title,titleColumns);
}

std::string flightListingLabel(const Json& log,double nowMs){
  std::vector<std::string> parts{flightListingTitle(log)};
  const auto& activity=value::property(log,"lastActivityMs");
  const auto& modified=value::property(log,"modifiedMs");
  if(activity.is_number())parts.push_back(relativeTimeText(activity.get<double>(),nowMs));
  else if(modified.is_number())parts.push_back(relativeTimeText(modified.get<double>(),nowMs));
  else if(const auto stamp=isoTimestampMs(stringField(log,"modified")))
    parts.push_back(relativeTimeText(*stamp,nowMs));
  if(const auto branch=stringField(log,"gitBranch");!branch.empty())parts.push_back(branch);
  if(const auto model=stringField(log,"model");!model.empty())
    parts.push_back(shortModelName(model));
  const auto& count=value::property(log,"messageCount");
  if(count.is_number_unsigned()||count.is_number_integer()){
    const auto value=count.get<long long>();
    if(value>0){
      const bool approximate=value::property(log,"messageCountApproximate")==true;
      parts.push_back((approximate?"~":"")+std::to_string(value)+
        (value==1?" message":" messages"));
    }
  }
  const auto state=stringField(log,"endState");
  if(state=="interrupted")parts.push_back("interrupted");
  else if(state=="error")parts.push_back("last reply failed");
  else if(state=="ended")parts.push_back("ended");
  std::string label;
  for(const auto& part:parts){
    if(!label.empty())label+=" · ";
    label+=part;
  }
  if(const auto slug=stringField(log,"slug");!slug.empty())label+="  ("+slug+")";
  return label;
}

std::string flightListingDetail(const Json& log,double nowMs){
  std::vector<std::string> parts;
  if(const auto slug=stringField(log,"slug");!slug.empty())parts.push_back(slug);
  if(const auto branch=stringField(log,"gitBranch");!branch.empty())parts.push_back(branch);
  if(const auto model=stringField(log,"model");!model.empty())
    parts.push_back(shortModelName(model));
  const auto& activity=value::property(log,"lastActivityMs");
  const auto& modified=value::property(log,"modifiedMs");
  if(activity.is_number())parts.push_back(relativeTimeText(activity.get<double>(),nowMs));
  else if(modified.is_number())parts.push_back(relativeTimeText(modified.get<double>(),nowMs));
  else if(const auto stamp=isoTimestampMs(stringField(log,"modified")))
    parts.push_back(relativeTimeText(*stamp,nowMs));
  const auto& count=value::property(log,"messageCount");
  if(count.is_number_unsigned()||count.is_number_integer()){
    const auto value=count.get<long long>();
    if(value>0){
      const bool approximate=value::property(log,"messageCountApproximate")==true;
      parts.push_back((approximate?"~":"")+std::to_string(value)+
        (value==1?" message":" messages"));
    }
  }
  std::string detail;
  for(const auto& part:parts){
    if(!detail.empty())detail+=" · ";
    detail+=part;
  }
  return detail;
}

std::string flightListingEndState(const Json& log){
  const auto state=stringField(log,"endState");
  if(state=="interrupted")return "interrupted";
  if(state=="error")return "last reply failed";
  if(state=="ended")return "ended";
  return {};
}

std::string flightSearchText(const Json& log){
  std::string text=flightListingTitle(log);
  for(const auto* key:{"slug","gitBranch","projectPath","firstPrompt","customTitle",
      "aiTitle","agentName","tag","sessionId"}){
    const auto field=stringField(log,key);
    if(!field.empty()){text+=' ';text+=field;}
  }
  return folded(oneLine(text));
}

std::vector<std::size_t> matchFlightLogs(const Json& logs,const std::string& query){
  std::vector<std::size_t> exact,partial;
  const auto needle=folded(value::trim(query));
  if(needle.empty()||!logs.is_array())return exact;
  std::vector<std::string> terms;
  for(std::size_t start=0;start<needle.size();){
    const auto end=needle.find(' ',start);
    const auto term=needle.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(!term.empty())terms.push_back(term);
    if(end==std::string::npos)break;
    start=end+1;
  }
  for(std::size_t index=0;index<logs.size();++index){
    const auto& log=logs[index];
    bool same=false;
    for(const auto* key:{"slug","customTitle","aiTitle","agentName","sessionId"})
      if(folded(value::trim(stringField(log,key)))==needle){same=true;break;}
    const auto id=folded(stringField(log,"sessionId"));
    if(!same&&needle.size()>=8&&id.starts_with(needle))same=true;
    if(same){exact.push_back(index);continue;}
    const auto haystack=flightSearchText(log);
    if(std::all_of(terms.begin(),terms.end(),[&haystack](const std::string& term){
        return haystack.find(term)!=std::string::npos;}))partial.push_back(index);
  }
  return exact.empty()?partial:exact;
}
}
