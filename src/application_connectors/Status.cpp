#include "application_connectors/Status.hpp"
#include "application_connectors/Bootstrap.hpp"
#include "connectors/Executable.hpp"
#include "connectors/Scrapling.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/JsonValue.hpp"
#include <curl/curl.h>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace batbox::application_connectors {
namespace {
std::string trim(std::string text){return value::trim(text);}
std::string unquote(std::string text){
  if(text.size()>=2&&((text.front()=='"'&&text.back()=='"')||
      (text.front()=='\''&&text.back()=='\'')))return text.substr(1,text.size()-2);
  return text;
}
std::string withoutComment(const std::string& line){
  bool single=false,doubled=false;
  for(std::size_t i=0;i<line.size();++i){
    const char c=line[i];
    if(c=='\''&&!doubled)single=!single;
    else if(c=='"'&&!single)doubled=!doubled;
    else if(c=='#'&&!single&&!doubled&&(i==0||line[i-1]==' '||line[i-1]=='\t'))
      return line.substr(0,i);
  }
  return line;
}
std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value.at(name).is_string()
    ?value.at(name).get<std::string>():"";
}
std::string path(const std::filesystem::path& value){return runtime::pathToUtf8(value);}
std::size_t receive(char* data,std::size_t size,std::size_t count,void* context){
  auto& output=*static_cast<std::string*>(context);
  try{output.append(data,size*count);return size*count;}catch(...){return 0;}
}
std::optional<std::string> distInfoVersion(const std::filesystem::path& sitePackages){
  std::error_code error;
  if(!std::filesystem::is_directory(sitePackages,error))return std::nullopt;
  for(std::filesystem::directory_iterator it(sitePackages,error),end;!error&&it!=end;
      it.increment(error)){
    const auto name=path(it->path().filename());
    constexpr std::string_view prefix="scrapling-",suffix=".dist-info";
    if(name.size()>prefix.size()+suffix.size()&&name.starts_with(prefix)&&
        name.ends_with(suffix))
      return name.substr(prefix.size(),name.size()-prefix.size()-suffix.size());
  }
  return std::nullopt;
}
std::optional<std::string> versionUnder(const std::filesystem::path& root){
  if(root.empty())return std::nullopt;
  if(auto found=distInfoVersion(root/"Lib"/"site-packages"))return found;
  std::error_code error;
  const auto lib=root/"lib";
  if(!std::filesystem::is_directory(lib,error))return std::nullopt;
  for(std::filesystem::directory_iterator it(lib,error),end;!error&&it!=end;
      it.increment(error)){
    if(!path(it->path().filename()).starts_with("python"))continue;
    if(auto found=distInfoVersion(it->path()/"site-packages"))return found;
  }
  return std::nullopt;
}
}

std::filesystem::path searxngSettingsPath(const application::Config& config){
  return application::configurationDirectory(config)/"searxng"/"settings.yml";
}

std::string discoveredSearxngEndpoint(const std::filesystem::path& settings){
  std::ifstream input(settings);
  if(!input)return {};
  std::string line,section,bindAddress="127.0.0.1";
  int port=8888;
  while(std::getline(input,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    line=withoutComment(line);
    if(trim(line).empty())continue;
    const bool indented=line.front()==' '||line.front()=='\t';
    const auto text=trim(line);
    if(!indented){
      section=text.ends_with(':')?text.substr(0,text.size()-1):std::string{};
      continue;
    }
    if(section!="server")continue;
    if(text.starts_with("bind_address:"))
      bindAddress=unquote(trim(text.substr(sizeof("bind_address:")-1)));
    else if(text.starts_with("port:")){
      const auto selected=unquote(trim(text.substr(sizeof("port:")-1)));
      try{std::size_t used=0;const auto value=std::stoi(selected,&used);
        if(used==selected.size()&&value>0&&value<=65535)port=value;
      }catch(...){}
    }
  }
  std::string host=bindAddress;
  if(host.empty()||host=="0.0.0.0"||host=="::"||host=="localhost"||host=="::1"||
      host=="127.0.0.1")host="127.0.0.1";
  else if(host.find(':')!=std::string::npos&&host.front()!='[')host="["+host+"]";
  return "http://"+host+":"+std::to_string(port)+"/search";
}

SearxngTarget searxngTarget(const application::Config& config,std::filesystem::path settings){
  SearxngTarget target;
  target.settings=settings.empty()?searxngSettingsPath(config):std::move(settings);
  if(!config.searxngUrl.empty()){
    target.url=config.searxngUrl;target.source=SearxngTarget::Source::Configured;
    return target;
  }
  if(auto discovered=discoveredSearxngEndpoint(target.settings);!discovered.empty()){
    target.url=std::move(discovered);target.source=SearxngTarget::Source::Discovered;
  }
  return target;
}

WebAvailability webAvailability(const application::Config& config){
  WebAvailability result;
  result.scraplingCommand=config.scraplingCommand;
  result.scrapling=connectors::findExecutable(config.scraplingCommand);
  result.searxng=searxngTarget(config);
  return result;
}

std::string webNotice(const WebAvailability& availability){
  const bool fetch=availability.fetchReady(),search=availability.searchConfigured();
  if(fetch&&search)return {};
  if(!fetch&&!search)return "Web search and fetch are not set up · run: batbox setup web";
  if(!fetch)return "Web fetch is not set up (Scrapling not found) · run: batbox setup web";
  return "Web search is not set up (no SearXNG configured) · run: batbox setup web";
}
std::string fetchUnavailableNote(const WebAvailability& availability){
  if(availability.fetchReady())return {};
  return "Unavailable on this machine: Scrapling is not installed, so calls fail. "
    "If the user needs a web page, tell them to run: batbox setup web";
}
std::string searchUnavailableNote(const WebAvailability& availability){
  if(availability.searchConfigured())return {};
  return std::string("No SearXNG instance is configured; calls try ")+defaultSearxngUrl+
    " and fail if nothing answers. If a call fails, tell the user to run: batbox setup web";
}

SearxngProbe probeSearxng(const std::string& url,std::chrono::milliseconds timeout){
  static const auto initialized=curl_global_init(CURL_GLOBAL_DEFAULT);
  SearxngProbe probe;
  if(initialized!=CURLE_OK){probe.error="HTTP initialization failed";return probe;}
  std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> http(curl_easy_init(),curl_easy_cleanup);
  if(!http){probe.error="Cannot create HTTP request";return probe;}
  const auto target=url+(url.find('?')==std::string::npos?"?":"&")+"q=batbox&format=json";
  std::string payload;
  curl_easy_setopt(http.get(),CURLOPT_URL,target.c_str());
  curl_easy_setopt(http.get(),CURLOPT_USERAGENT,"batbox-doctor");
  curl_easy_setopt(http.get(),CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(http.get(),CURLOPT_CONNECTTIMEOUT_MS,
    static_cast<long>(std::min<long long>(timeout.count(),2000)));
  curl_easy_setopt(http.get(),CURLOPT_TIMEOUT_MS,static_cast<long>(timeout.count()));
  curl_easy_setopt(http.get(),CURLOPT_WRITEFUNCTION,receive);
  curl_easy_setopt(http.get(),CURLOPT_WRITEDATA,&payload);
  const auto result=curl_easy_perform(http.get());
  if(result!=CURLE_OK){probe.error=curl_easy_strerror(result);return probe;}
  probe.reachable=true;
  curl_easy_getinfo(http.get(),CURLINFO_RESPONSE_CODE,&probe.status);
  if(probe.status==403){probe.error="JSON output is disabled (HTTP 403)";return probe;}
  if(probe.status<200||probe.status>=300){
    probe.error="HTTP "+std::to_string(probe.status);return probe;}
  try{
    const auto response=value::parse(payload);
    if(response.is_object()&&response.contains("results")&&response.at("results").is_array()){
      probe.json=true;probe.results=response.at("results").size();return probe;
    }
  }catch(...){}
  probe.error="the response is not SearXNG JSON";
  return probe;
}

std::optional<std::string> scraplingVersion(const std::filesystem::path& executable){
  std::error_code error;
  std::vector<std::filesystem::path> roots{executable.parent_path().parent_path()};
  const auto resolved=std::filesystem::canonical(executable,error);
  if(!error)roots.push_back(resolved.parent_path().parent_path());
  std::ifstream script(executable);
  std::string first;
  if(script&&std::getline(script,first)&&first.starts_with("#!")){
    auto interpreter=trim(first.substr(2));
    if(const auto space=interpreter.find(' ');space!=std::string::npos)
      interpreter.resize(space);
    if(!interpreter.empty()&&interpreter.find("python")!=std::string::npos){
      const auto python=runtime::pathFromUtf8(interpreter);
      roots.push_back(python.parent_path().parent_path());
      const auto real=std::filesystem::canonical(python,error);
      if(!error)roots.push_back(real.parent_path().parent_path());
    }
  }
  for(const auto& root:roots)if(auto found=versionUnder(root))return found;
  return std::nullopt;
}

std::optional<std::string> installedScraplingVersion(const std::string& command){
  const auto executable=connectors::findExecutable(command);
  return executable?scraplingVersion(*executable):std::nullopt;
}

ScraplingProbe probeScrapling(const application::Config& config,std::chrono::milliseconds timeout){
  ScraplingProbe probe;
  try{
    Scrapling client(scraplingServerArgv(config),timeout);
    try{
      const auto offered=client.advertisedGadgets();
      probe.started=true;
      for(const auto& tool:offered)probe.tools.push_back(tool.name);
      if(const auto* tool=chooseScraplingTool(offered,scraplingPlainFetchTools()))
        probe.fetchTool=tool->name;
    }catch(...){client.shutdown();throw;}
    client.shutdown();
  }catch(const std::exception& error){probe.error=error.what();}
  return probe;
}

HelperCheck checkScrapling(const WebAvailability& availability,
    const std::optional<ScraplingProbe>& probe){
  HelperCheck check;check.name="Scrapling";
  if(availability.scrapling){
    check.detail=path(*availability.scrapling);
    const auto version=scraplingVersion(*availability.scrapling);
    if(version)check.detail+=" · "+*version;
    if(!probe){
      check.ok=true;check.state="ready";
      check.detail+=" · starts on first web fetch";
      return check;
    }
    if(!probe->started){
      check.state="not starting";
      check.detail+=" · "+probe->error;
      check.fix="batbox setup web, or set scrapling_command to a scrapling with its MCP "
        "server (scrapling[ai])";
      return check;
    }
    if(probe->fetchTool.empty()){
      check.state="incompatible";
      check.detail+=" · "+scraplingToolMissingMessage(scraplingPlainFetchTools(),
        probe->tools,version,false);
      check.fix=std::string("batbox setup web (Batbox needs Scrapling >= ")+
        minimumScraplingVersion+")";
      return check;
    }
    check.ok=true;check.state="ready";
    check.detail+=" · MCP server answers · fetch uses "+probe->fetchTool;
    return check;
  }
  check.state="missing";
  check.detail="\""+availability.scraplingCommand+"\" not found";
  check.fix="batbox setup web, or set scrapling_command to the scrapling executable";
  return check;
}
HelperCheck checkSearxng(const WebAvailability& availability,const SearxngProbe& probe){
  HelperCheck check;check.name="SearXNG";
  const auto& target=availability.searxng;
  std::string source;
  if(target.source==SearxngTarget::Source::Configured)source="searxng_url";
  else if(target.source==SearxngTarget::Source::Discovered)source=path(target.settings);
  else source="default, nothing configured";
  check.detail=target.url+" ("+source+")";
  if(probe.json){
    check.ok=true;check.state="reachable";
    check.detail+=" · JSON on · "+std::to_string(probe.results)+" results for \"batbox\"";
    return check;
  }
  if(!probe.reachable){
    check.state="unreachable";
    check.detail+=" · "+probe.error;
    check.fix=target.source==SearxngTarget::Source::Default?
      "batbox setup web, or set searxng_url to your instance's /search endpoint":
      "start that instance (batbox setup web installs one as a user service), or fix searxng_url";
    return check;
  }
  check.state=probe.status==403?"JSON off":"not SearXNG JSON";
  check.detail+=" · "+probe.error;
  check.fix=probe.status==403?
    "add json to search.formats in the instance's settings.yml, then restart it":
    "point searxng_url at the full /search endpoint of a SearXNG instance";
  return check;
}
std::vector<HelperCheck> checkWebHelpers(const application::Config& config){
  const auto availability=webAvailability(config);
  std::future<ScraplingProbe> scrapling;
  if(availability.scrapling)
    scrapling=std::async(std::launch::async,[&config]{return probeScrapling(config);});
  const auto searxng=probeSearxng(availability.searxng.url);
  std::optional<ScraplingProbe> probe;
  if(scrapling.valid())probe=scrapling.get();
  return {checkScrapling(availability,probe),checkSearxng(availability,searxng)};
}

Json configuredMcpServers(const application::Config& config){
  if(config.mcpServersConfigured)return config.mcpServers;
  std::ifstream input(application::configurationDirectory(config)/"mcp.json");
  if(!input)return Json::object();
  const auto document=Json::parse(input);
  if(!document.is_object())throw std::invalid_argument("Private MCP config must be an object");
  const auto servers=document.value("mcpServers",document);
  if(!servers.is_object())throw std::invalid_argument("Private MCP servers must be an object");
  return servers;
}
std::vector<HelperCheck> checkMcpConfiguration(const application::Config& config){
  std::vector<HelperCheck> checks;
  Json servers;
  try{servers=configuredMcpServers(config);}
  catch(const std::exception& error){
    HelperCheck check;check.name="mcp.json";check.state="invalid";check.detail=error.what();
    check.fix="fix the JSON in "+path(application::configurationDirectory(config)/"mcp.json");
    return {check};
  }
  for(auto it=servers.begin();it!=servers.end();++it){
    HelperCheck check;check.name=it.key();
    const auto& entry=it.value();
    const auto type=field(entry,"type");
    if(!entry.is_object()){
      check.state="invalid";check.detail="entry is not an object";
      check.fix="give it a command (stdio) or url (http)";
    }else if(type=="http"||type=="sse"||!field(entry,"url").empty()){
      check.ok=true;check.state="remote";check.detail=field(entry,"url")+" · not contacted";
    }else if(!type.empty()&&type!="stdio"){
      check.state="invalid";check.detail="unknown transport \""+type+"\"";
      check.fix="use stdio, http or sse";
    }else{
      const auto command=field(entry,"command");
      if(command.empty()){
        check.state="invalid";check.detail="no command";
        check.fix="set its command in the MCP configuration";
      }else if(auto found=connectors::findExecutable(command)){
        check.ok=true;check.state="ready";check.detail=path(*found);
      }else{
        check.state="missing";check.detail="\""+command+"\" not found";
        check.fix="install "+command+" or fix its command in the MCP configuration";
      }
    }
    checks.push_back(std::move(check));
  }
  return checks;
}
HelperCheck mcpServerCheck(const std::string& name,const std::string& type,
    const std::string& error){
  HelperCheck check;check.name=name;check.state=type;check.detail=error;
  if(type=="connected")check.ok=true;
  else if(type=="failed")check.fix="check its command in mcp.json, then /mcp reconnect "+name;
  else if(type=="needs-auth")check.fix="authenticate with /mcp";
  else if(type=="disabled")check.fix="/mcp enable "+name;
  else if(type=="pending")check.detail=check.detail.empty()?"still connecting":check.detail;
  return check;
}

std::string formatDoctor(const std::vector<HelperCheck>& web,
    const std::vector<HelperCheck>& mcp,const std::string& configurationError,
    const std::string& title){
  std::ostringstream out;
  const auto row=[&](const HelperCheck& check){
    out<<"  "<<(check.ok?"[ok]  ":"[--]  ")<<check.name<<": "<<check.state;
    if(!check.detail.empty())out<<" · "<<check.detail;
    out<<'\n';
    if(!check.fix.empty())out<<"        fix: "<<check.fix<<'\n';
  };
  out<<title<<'\n';
  if(!configurationError.empty())
    out<<"\nConfiguration: "<<configurationError<<"\n  (checks below use defaults)\n";
  out<<"\nWeb helpers (optional; Batbox works without them)\n";
  for(const auto& check:web)row(check);
  out<<"\nMCP servers\n";
  if(mcp.empty())out<<"  none configured\n";
  for(const auto& check:mcp)row(check);
  return out.str();
}
} // namespace batbox::application_connectors
