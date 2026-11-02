// Optional web helpers: lazy Scrapling, clear unavailable errors, SearXNG
// discovery without personal fallbacks, and the startup availability line.
#include "application_connectors/Bootstrap.hpp"
#include "application_connectors/Status.hpp"
#include "connectors/Executable.hpp"
#include "gadgets/web/Web.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace batbox;
namespace ac=batbox::application_connectors;
namespace fs=std::filesystem;
namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
bool contains(const std::string& text,const std::string& part){
  return text.find(part)!=std::string::npos;
}
void write(const fs::path& path,const std::string& text){
  fs::create_directories(path.parent_path());
  std::ofstream(path)<<text;
}
std::string thrown(const std::function<void()>& action){
  try{action();}catch(const std::exception& error){return error.what();}
  return {};
}
int closedPort(){
  int fd=::socket(AF_INET,SOCK_STREAM,0);
  sockaddr_in address{};address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  ::bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address));
  socklen_t length=sizeof(address);
  ::getsockname(fd,reinterpret_cast<sockaddr*>(&address),&length);
  const int port=ntohs(address.sin_port);
  ::close(fd);
  return port;
}
gadgets::web::UI ui(){
  gadgets::web::UI result;
  result.getGadgetUseSummary=[](const Json&)->Json{return Json();};
  result.renderUse=[](const Json&,const Json&)->Json{return Json();};
  result.renderProgress=[](const Json&,const Json&)->Json{return Json();};
  result.renderResult=[](const Json&,const Json&,const Json&)->Json{return Json();};
  return result;
}

void lazyScrapling(const fs::path& root,const fs::path& fixture){
  const auto marker=root/"scrapling-started";
  Scrapling client({"/bin/sh","-c","touch '"+marker.string()+"' && exec '"+fixture.string()+"'"},
    std::chrono::seconds(10));
  check(!client.running()&&!fs::exists(marker),"Constructing Scrapling started a process");
  client.listGadgets();
  check(client.running()&&fs::exists(marker),"The first request did not start Scrapling");
  fs::remove(marker);
  client.callGadget("make_request",{{"url","https://example.invalid"}});
  check(!fs::exists(marker),"A second request started another Scrapling process");
  client.shutdown();
  check(!client.running(),"Shutdown left Scrapling marked running");
  check(contains(thrown([&]{client.listGadgets();}),"shutting down"),
    "A request after shutdown restarted Scrapling");
}

void missingScrapling(){
  Scrapling missing({"/nonexistent/batbox-test/scrapling","mcp"});
  check(!missing.running(),"A missing Scrapling was reported running");
  const auto first=thrown([&]{missing.listGadgets();});
  check(contains(first,"not installed")&&contains(first,"batbox setup web"),
    "Missing Scrapling error lacks the fix: "+first);
  check(contains(thrown([&]{missing.callGadget("make_request",Json::object());}),
    "batbox setup web"),"A retry after a missing Scrapling changed its error");

  gadgets::web::FetchServices fetch;
  fetch.connector=&missing;fetch.ui=ui();
  fetch.permissionRule=[](const Json&,const std::string&,const std::string&)->std::optional<Json>{
    return std::nullopt;};
  fetch.skipPreflight=[]{return true;};
  fetch.nowMs=[]{return 1.0;};
  fetch.querySmallModel=[](const Json&,std::stop_token){return Json::object();};
  fetch.unavailableNote=[]{return std::string("FETCH-UNAVAILABLE-NOTE");};
  auto gadget=gadgets::web::makeWebFetchGadget(fetch);
  check(contains(gadget.poetry({}).get(),"FETCH-UNAVAILABLE-NOTE"),
    "WebFetch description lacks the unavailable note");
  const auto error=thrown([&]{
    gadget.call({{"url","https://docs.example.org/page"},{"prompt","Summarize"}},{},{},Json(),{}).get();});
  check(contains(error,"batbox setup web"),"WebFetch without Scrapling did not fail clearly: "+error);
}

void unreachableSearxng(){
  const auto endpoint="http://127.0.0.1:"+std::to_string(closedPort())+"/search";
  Searxng searxng(endpoint);
  gadgets::web::SearchServices search;
  search.connector=&searxng;search.ui=ui();
  search.localMonthYear=[]{return std::string("November 2026");};
  search.unavailableNote=[]{return std::string("SEARCH-UNAVAILABLE-NOTE");};
  auto gadget=gadgets::web::makeWebSearchGadget(search);
  check(contains(gadget.poetry({}).get(),"SEARCH-UNAVAILABLE-NOTE"),
    "WebSearch description lacks the unavailable note");
  const auto error=thrown([&]{gadget.call({{"query","batbox web helpers"}},{},{},Json(),{}).get();});
  check(contains(error,"not reachable")&&contains(error,endpoint)&&
    contains(error,"batbox setup web"),"Unreachable SearXNG error is unclear: "+error);
  const auto probe=ac::probeSearxng(endpoint,std::chrono::seconds(2));
  check(!probe.reachable&&!probe.json&&!probe.error.empty(),"Probe reached a closed port");
}

void discovery(const fs::path& root){
  check(ac::discoveredSearxngEndpoint(root/"absent.yml").empty(),"Missing settings were discovered");
  write(root/"a.yml","# port: 1\nuse_default_settings:\n  engines:\n    keep_only:\n      - duckduckgo\n"
    "general:\n  port: 2\nserver:\n  port: 18890  # local\n  bind_address: \"0.0.0.0\"\n"
    "  secret_key: \"x # not a comment\"\nsearch:\n  formats:\n    - json\n");
  check(ac::discoveredSearxngEndpoint(root/"a.yml")=="http://127.0.0.1:18890/search",
    "Server block port was not discovered: "+ac::discoveredSearxngEndpoint(root/"a.yml"));
  write(root/"b.yml","server:\n  secret_key: abc\n");
  check(ac::discoveredSearxngEndpoint(root/"b.yml")==ac::defaultSearxngUrl,
    "SearXNG defaults were not applied");
  write(root/"c.yml","server:\n  bind_address: '192.168.4.2'\n  port: '9000'\n");
  check(ac::discoveredSearxngEndpoint(root/"c.yml")=="http://192.168.4.2:9000/search",
    "A LAN bind address was not used");

  // A leftover personal settings path must never be read.
  const auto home=root/"home";
  write(home/".config"/"claude-private"/"searxng"/"settings.yml","server:\n  port: 9999\n");
  ::setenv("HOME",home.c_str(),1);
  application::Config config;
  config.configDirectory=root/"config";config.configPath=config.configDirectory/"native-client.json";
  config.scraplingCommand="/nonexistent/batbox-test/scrapling";
  config.mcpServersConfigured=true;
  auto target=ac::searxngTarget(config);
  check(target.url==ac::defaultSearxngUrl&&target.source==ac::SearxngTarget::Source::Default,
    "Discovery used a path outside the Batbox configuration: "+target.url);
  event_loop::Loop loop;
  auto bundle=ac::bootstrap(loop,config,{},{},true);
  check(bundle->searxngEndpoint==ac::defaultSearxngUrl,"Bootstrap used a personal SearXNG path");
  check(bundle->scrapling&&!bundle->scrapling->running(),"Bootstrap started Scrapling");
  check(bundle->diagnostics.empty(),"Bootstrap reported a diagnostic for missing helpers");
  write(config.configDirectory/"searxng"/"settings.yml","server:\n  port: 18891\n");
  check(ac::bootstrap(loop,config,{},{},true)->searxngEndpoint=="http://127.0.0.1:18891/search",
    "Bootstrap ignored the Batbox SearXNG settings file");
  config.searxngUrl="http://search.example.org/search";
  target=ac::searxngTarget(config);
  check(target.url==config.searxngUrl&&target.source==ac::SearxngTarget::Source::Configured,
    "searxng_url did not win over discovery");
}

void availability(const fs::path& root,const fs::path& fixture){
  application::Config config;
  config.configDirectory=root/"empty-config";
  config.scraplingCommand="/nonexistent/batbox-test/scrapling";
  auto web=ac::webAvailability(config);
  check(ac::webNotice(web)=="Web search and fetch are not set up · run: batbox setup web",
    "Unexpected notice: "+ac::webNotice(web));
  check(contains(ac::fetchUnavailableNote(web),"batbox setup web")&&
    contains(ac::searchUnavailableNote(web),ac::defaultSearxngUrl),"Model notes lack the fix");
  config.scraplingCommand=fixture.string();
  web=ac::webAvailability(config);
  check(contains(ac::webNotice(web),"Web search is not set up")&&ac::fetchUnavailableNote(web).empty(),
    "Fetch should be ready with an executable Scrapling");
  config.searxngUrl="http://127.0.0.1:1/search";
  check(ac::webNotice(ac::webAvailability(config)).empty(),"Configured helpers still showed a notice");
  check(connectors::findExecutable("sh").has_value(),"PATH lookup failed for sh");
  check(!connectors::findExecutable("batbox-no-such-helper").has_value(),"Lookup invented a helper");

  const auto scrapling=ac::checkScrapling(ac::webAvailability(config));
  check(scrapling.ok&&contains(scrapling.detail,fixture.string()),"Scrapling check missed the fixture");
  const auto down=ac::checkSearxng(ac::webAvailability(config),ac::SearxngProbe{false,false,0,0,"refused"});
  check(!down.ok&&down.state=="unreachable"&&!down.fix.empty(),"Unreachable SearXNG check lacks a fix");
  const auto off=ac::checkSearxng(ac::webAvailability(config),ac::SearxngProbe{true,false,403,0,"off"});
  check(!off.ok&&off.state=="JSON off"&&contains(off.fix,"json"),"JSON-off check lacks its fix");

  config.mcpServersConfigured=true;
  config.mcpServers={{"present",{{"command","sh"}}},{"absent",{{"command","batbox-no-such-helper"}}},
    {"remote",{{"type","http"},{"url","https://mcp.example.org"}}}};
  const auto mcp=ac::checkMcpConfiguration(config);
  check(mcp.size()==3,"MCP checks lost a server");
  for(const auto& item:mcp){
    if(item.name=="absent")check(!item.ok&&contains(item.fix,"batbox-no-such-helper"),"Missing MCP command");
    else check(item.ok,"MCP server "+item.name+" should pass");
  }
  check(contains(ac::formatDoctor(std::vector<ac::HelperCheck>{scrapling,down},mcp),"fix: "),"Doctor text lacks fixes");
}
}

int main(int argc,char** argv){
  try{
    check(argc==2,"Scrapling fixture required");
    const fs::path fixture=fs::absolute(argv[1]);
    const auto root=fs::temp_directory_path()/("batbox-web-connectors-"+std::to_string(::getpid()));
    fs::remove_all(root);fs::create_directories(root);
    lazyScrapling(root,fixture);
    missingScrapling();
    unreachableSearxng();
    discovery(root);
    availability(root,fixture);
    fs::remove_all(root);
    std::cout<<"lazy Scrapling, unavailable web Gadgets, discovery and availability passed\n";
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
