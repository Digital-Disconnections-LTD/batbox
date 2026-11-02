// Optional web helpers: lazy Scrapling, fetch tool discovery, clear unavailable
// errors, SearXNG discovery without personal fallbacks, the startup availability
// line, the doctor's Scrapling handshake and the SearXNG answer shape.
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
  client.callGadget("get",{{"url","https://example.invalid"}});
  check(!fs::exists(marker),"A second request started another Scrapling process");
  client.shutdown();
  check(!client.running(),"Shutdown left Scrapling marked running");
  check(contains(thrown([&]{client.listGadgets();}),"shutting down"),
    "A request after shutdown restarted Scrapling");
}

fs::path wrapper(const fs::path& path,const std::string& body){
  write(path,"#!/bin/sh\n"+body+"\n");
  fs::permissions(path,fs::perms::owner_all,fs::perm_options::replace);
  return path;
}
std::string firstText(const Json& result){
  return result.at("content").at(0).at("text").get<std::string>();
}
gadgets::web::FetchServices fetchServices(Scrapling& connector){
  gadgets::web::FetchServices fetch;
  fetch.connector=&connector;fetch.ui=ui();
  fetch.permissionRule=[](const Json&,const std::string&,const std::string&)->std::optional<Json>{
    return std::nullopt;};
  fetch.skipPreflight=[]{return true;};
  fetch.nowMs=[]{return 1.0;};
  fetch.querySmallModel=[](const Json&,std::stop_token){return Json::object();};
  fetch.unavailableNote=[]{return std::string();};
  return fetch;
}

void fetchToolDiscovery(const fs::path& fixture){
  {
    Scrapling current({fixture.string()},std::chrono::seconds(10));
    check(current.fetchGadget().name=="get","Scrapling 0.4.7 tools did not select get");
    const auto page=gadgets::web::fetchThroughScrapling("https://example.org/page",current,{});
    check(page.code==200&&page.finalUrl=="https://example.org/page"&&
      contains(page.content,"Fetched with get as markdown"),"get fetch content: "+page.content);
    gadgets::web::fetchThroughScrapling("https://example.org/again",current,{});
    check(firstText(current.callGadget("fixture_tools_list_count",Json::object()))=="1",
      "tools/list ran more than once for one Scrapling process");
    const auto failed=thrown([&]{
      gadgets::web::fetchThroughScrapling("https://error.example/page",current,{});});
    check(failed=="Scrapling fetch failed: Failed to fetch https://error.example/page: connection refused",
      "A Scrapling tool error was not shown as its text: "+failed);
    const auto unknown=current.callGadget("make_request",{{"url","https://example.org/page"}});
    check(unknown.value("isError",false)&&firstText(unknown)=="Unknown tool: make_request",
      "An unadvertised tool did not return the server's tool error");
  }
  {
    Scrapling newer({fixture.string(),"--tools=0.4.15"},std::chrono::seconds(10));
    check(newer.fetchGadget().name=="make_request","Scrapling 0.4.15 tools did not select make_request");
    const auto page=gadgets::web::fetchThroughScrapling("https://example.org/page",newer,{});
    check(contains(page.content,"Fetched with make_request as markdown"),
      "make_request fetch content: "+page.content);
  }
  {
    Scrapling old({fixture.string(),"--tools=unknown"},std::chrono::seconds(10),
      []{return std::optional<std::string>("0.2.9");});
    const auto expected="Scrapling 0.2.9 has no tool named make_request or get (it offers: "
      "browser_open, browser_fetch, browser_screenshot, close_session); Batbox needs "
      "Scrapling >= 0.3. Run: batbox setup web";
    const auto error=thrown([&]{old.fetch("https://example.org/page");});
    check(error==expected,"Unclear missing fetch tool error: "+error);
    bool typed=false;
    try{old.fetchGadget();}
    catch(const ScraplingToolMissing& missing){
      typed=missing.wanted==scraplingPlainFetchTools()&&missing.offered.size()==4;
    }
    check(typed,"A missing fetch tool did not raise ScraplingToolMissing");
    auto gadget=gadgets::web::makeWebFetchGadget(fetchServices(old));
    const auto shown=thrown([&]{
      gadget.call({{"url","https://docs.example.org/page"},{"prompt","Summarize"}},{},{},Json(),{}).get();});
    check(shown==expected,"WebFetch did not show the missing tool error: "+shown);
  }
  {
    Scrapling anonymous({fixture.string(),"--tools=unknown"},std::chrono::seconds(10));
    const auto error=thrown([&]{anonymous.fetch("https://example.org/page");});
    check(error.rfind("This Scrapling has no tool named make_request or get",0)==0,
      "A missing tool error without a version is unclear: "+error);
  }
}

void doctorHandshake(const fs::path& root,const fs::path& fixture){
  application::Config config;
  config.configDirectory=root/"doctor-config";
  config.scraplingCommand=fixture.string();
  auto probe=ac::probeScrapling(config,std::chrono::seconds(10));
  check(probe.started&&probe.fetchTool=="get"&&probe.tools.size()==10&&probe.error.empty(),
    "The doctor probe misread Scrapling 0.4.7 tools: "+probe.error);
  const auto ready=ac::checkScrapling(ac::webAvailability(config),probe);
  check(ready.ok&&ready.state=="ready"&&contains(ready.detail,"fetch uses get"),
    "A compatible Scrapling did not pass: "+ready.detail);

  config.scraplingCommand=wrapper(root/"old-scrapling",
    "exec '"+fixture.string()+"' --tools=unknown \"$@\"").string();
  probe=ac::probeScrapling(config,std::chrono::seconds(10));
  check(probe.started&&probe.fetchTool.empty()&&probe.tools.size()==4,
    "The doctor probe missed an incompatible tool set");
  const auto incompatible=ac::checkScrapling(ac::webAvailability(config),probe);
  check(!incompatible.ok&&incompatible.state=="incompatible"&&
    contains(incompatible.detail,"has no tool named make_request or get")&&
    contains(incompatible.detail,"it offers: browser_open")&&
    !contains(incompatible.detail,"Run:")&&
    contains(incompatible.fix,"batbox setup web")&&contains(incompatible.fix,">= 0.3"),
    "Incompatible Scrapling check is unclear: "+incompatible.detail+" / "+incompatible.fix);
  check(contains(ac::formatDoctor({incompatible},{}),"[--]  Scrapling: incompatible"),
    "Doctor text does not flag the incompatible Scrapling");
  config.searxngUrl="http://127.0.0.1:"+std::to_string(closedPort())+"/search";
  const auto web=ac::checkWebHelpers(config);
  check(web.size()==2&&web.front().state=="incompatible",
    "checkWebHelpers did not run the Scrapling handshake");

  config.scraplingCommand=wrapper(root/"silent-scrapling","exit 0").string();
  probe=ac::probeScrapling(config,std::chrono::seconds(10));
  const auto silent=ac::checkScrapling(ac::webAvailability(config),probe);
  check(!probe.started&&!silent.ok&&silent.state=="not starting"&&
    contains(silent.detail,"could not start")&&contains(silent.fix,"batbox setup web"),
    "A Scrapling without an MCP server passed: "+silent.detail);
}

void scraplingBanner(const fs::path& root,const fs::path& fixture){
  Scrapling noisy({fixture.string(),"--banner"},std::chrono::seconds(10));
  const auto page=gadgets::web::fetchThroughScrapling("https://example.org/page",noisy,{});
  check(page.code==200&&contains(page.content,"Fetched with get"),
    "Download lines on Scrapling's stdout broke the fetch: "+page.content);
  noisy.shutdown();
  application::Config config;
  config.configDirectory=root/"banner-config";
  config.scraplingCommand=wrapper(root/"banner-scrapling",
    "exec '"+fixture.string()+"' --banner \"$@\"").string();
  const auto probe=ac::probeScrapling(config,std::chrono::seconds(10));
  check(probe.started&&probe.fetchTool=="get","The doctor failed a Scrapling that prints "
    "download lines before its first answer: "+probe.error);
}

void scraplingMcp2(){
  application::Config config;
  config.scraplingCommand="sh";
  ac::ScraplingProbe probe;
  probe.error="Scrapling could not start: MCP server output ended before the response; server "
    "stderr: Traceback (most recent call last):\n  File \"scrapling/core/ai.py\", line 3\n"
    "ModuleNotFoundError: No module named 'mcp.server.fastmcp'. This is mcp 2.x";
  const auto broken=ac::checkScrapling(ac::webAvailability(config),probe);
  check(!broken.ok&&broken.state=="not starting"&&
    contains(broken.detail,"imports mcp.server.fastmcp, which the mcp 2 package removed")&&
    contains(broken.detail,"before 0.4.13 needs mcp < 2")&&!contains(broken.detail,"Traceback")&&
    contains(broken.fix,"batbox setup web --upgrade")&&contains(broken.fix,"mcp<2"),
    "A Scrapling broken by mcp 2 is unclear: "+broken.detail+" / "+broken.fix);
}

SearxngProblem searxngProblem(long status,const std::string& type,const std::string& body){
  try{searxngResults("http://127.0.0.1:8888/search",status,type,body);}
  catch(const SearxngResponseError& error){
    check(contains(error.what(),"SearXNG at http://127.0.0.1:8888/search: ")&&
      contains(error.what(),error.problem.detail)&&contains(error.what(),"Fix: "+error.problem.fix),
      std::string("SearXNG error text lacks its parts: ")+error.what());
    return error.problem;
  }
  throw std::runtime_error("SearXNG answer was accepted: HTTP "+std::to_string(status)+" "+body);
}

void searxngShape(){
  using Kind=SearxngProblem::Kind;
  const Json good={{"query","bats"},{"number_of_results",0},{"suggestions",nullptr},
    {"results",Json::array({
      {{"url","https://a.example/1"},{"title",nullptr},{"content",nullptr},{"engine",nullptr},
        {"publishedDate",nullptr}},
      {{"url","https://b.example/2"},{"title","Two"},{"content","Text"},{"engine","wikipedia"},
        {"publishedDate","2026-10-01T00:00:00"}},
      {{"url","https://c.example/3"}},
      {{"title","No url"}},{{"url",nullptr},{"title","Null url"}},"stray",nullptr})}};
  const auto rows=searxngResults("http://s/search",200,"application/json",good.dump());
  check(rows.size()==3&&rows.at(0).at("url")=="https://a.example/1"&&
    rows.at(2).at("url")=="https://c.example/3","Usable SearXNG rows were lost or invented: "+rows.dump());
  check(searxngResults("http://s/search",200,"",R"({"results":[]})").empty(),
    "An empty result list was rejected");
  check(searxngResults("http://s/search",200,"",R"({"results":[{"url":"https://x.example"}]})").size()==1,
    "A minimal result was rejected");

  const std::string page="<!DOCTYPE html>\n<html class=\"no-js\">\n<head><title>403 Forbidden</title>"
    "</head>\n<body><h1>Forbidden</h1><p>You don't have the permission to access the requested "
    "resource. It is either read-protected or not readable by the server.</p></body></html>";
  auto problem=searxngProblem(403,"text/html; charset=utf-8",page);
  check(problem.kind==Kind::JsonOff&&
    contains(problem.detail,"expected HTTP 200 with JSON search results, got HTTP 403 "
      "(text/html; charset=utf-8): \"<!DOCTYPE html> <html class=\"no-js\"> <head>")&&
    contains(problem.detail,"...\"")&&contains(problem.fix,"search.formats")&&
    contains(problem.fix,"lacks json")&&contains(problem.fix,"batbox setup web"),
    "403 HTML answer is unclear: "+problem.detail+" / "+problem.fix);
  const auto quoted=problem.detail.substr(problem.detail.find(": \"")+3);
  check(value::stringLength(quoted)==120+4,"The body excerpt is not 120 characters: "+quoted);

  problem=searxngProblem(200,"text/html","<html><body>Batbox proxy login</body></html>");
  check(problem.kind==Kind::NotJson&&problem.detail=="expected a JSON body, got HTTP 200 "
    "(text/html): \"<html><body>Batbox proxy login</body></html>\""&&
    contains(problem.fix,"search.formats")&&contains(problem.fix,"searxng_url"),
    "Non-JSON answer is unclear: "+problem.detail);

  problem=searxngProblem(200,"application/json",R"({"query":"bats","answers":[]})");
  check(problem.kind==Kind::NoResults&&problem.detail=="expected a JSON object with a "
    "\"results\" array, got an object without \"results\" (keys: query, answers)"&&
    contains(problem.fix,"batbox setup web"),"Missing results answer is unclear: "+problem.detail);
  problem=searxngProblem(200,"application/json",R"({"results":"none"})");
  check(problem.kind==Kind::NoResults&&contains(problem.detail,"got \"results\" as a string"),
    "String results answer is unclear: "+problem.detail);
  problem=searxngProblem(200,"application/json",R"([{"url":"https://x.example"}])");
  check(problem.kind==Kind::NoResults&&contains(problem.detail,"got an array"),
    "Array answer is unclear: "+problem.detail);
  problem=searxngProblem(200,"application/json","null");
  check(problem.kind==Kind::NoResults&&contains(problem.detail,"got null"),
    "Null answer is unclear: "+problem.detail);

  problem=searxngProblem(200,"application/json",
    R"({"results":[{"link":"https://x.example","title":"t"},{"href":"https://y.example"}]})");
  check(problem.kind==Kind::NoUrl&&problem.detail=="expected each result to have a \"url\" "
    "string, got 2 results without one (first result: keys link, title)"&&
    contains(problem.fix,"batbox setup web"),"Results without url are unclear: "+problem.detail);
  problem=searxngProblem(200,"application/json",R"({"results":[null]})");
  check(problem.kind==Kind::NoUrl&&contains(problem.detail,"1 result without one (first result: null)"),
    "A null result is unclear: "+problem.detail);

  problem=searxngProblem(500,"","");
  check(problem.kind==Kind::HttpStatus&&problem.detail=="expected HTTP 200 with JSON search "
    "results, got HTTP 500 (no content type): an empty body"&&contains(problem.fix,"running SearXNG"),
    "Empty 500 answer is unclear: "+problem.detail);
  problem=searxngProblem(308,"text/html","<a href=\"/search\">Redirecting</a>");
  check(problem.kind==Kind::HttpStatus&&contains(problem.fix,"full /search endpoint"),
    "Redirect answer lacks its fix: "+problem.fix);
  std::string wide;
  for(int i=0;i<200;++i)wide+="\xc3\xa9";
  problem=searxngProblem(502,"text/plain","\x1b[31m"+wide+"\xc2\x9b" "2J");
  check(!contains(problem.detail,"\x1b")&&!contains(problem.detail,"\xc2\x9b")&&
    contains(problem.detail,"[31m"+wide.substr(0,2*116)+"...\""),
    "Body excerpt kept control characters or split a character: "+problem.detail);
}

void searxngDoctor(){
  application::Config config;
  config.configDirectory="/nonexistent/batbox-test/config";
  config.scraplingCommand="/nonexistent/batbox-test/scrapling";
  config.searxngUrl="http://127.0.0.1:8888/search";
  const auto web=ac::webAvailability(config);
  ac::SearxngProbe ready{true,true,200,7,"","2026.10.25+54ca1d9cf",""};
  auto line=ac::formatDoctor({ac::checkSearxng(web,ready)},{});
  check(contains(line,"  [ok]  SearXNG: ready · http://127.0.0.1:8888/search (searxng_url) · "
    "SearXNG 2026.10.25+54ca1d9cf · 7 results for \"batbox\"\n"),"Ready doctor line: "+line);
  ready.version.clear();
  line=ac::formatDoctor({ac::checkSearxng(web,ready)},{});
  check(contains(line,"  [ok]  SearXNG: ready · http://127.0.0.1:8888/search (searxng_url) · "
    "7 results for \"batbox\"\n"),"Ready doctor line without a version: "+line);

  const auto problem=searxngProblem(200,"text/html","<html>login</html>");
  const ac::SearxngProbe shape{true,false,200,0,problem.detail,"",problem.fix};
  const auto mismatch=ac::checkSearxng(web,shape);
  line=ac::formatDoctor({mismatch},{});
  check(!mismatch.ok&&mismatch.state=="shape mismatch"&&
    contains(line,"  [--]  SearXNG: shape mismatch · http://127.0.0.1:8888/search (searxng_url) · "
      "expected a JSON body, got HTTP 200 (text/html): \"<html>login</html>\"\n        fix: check "
      "that searxng_url names a SearXNG /search endpoint with json in search.formats"),
    "Shape mismatch doctor line: "+line);
  const auto forbidden=searxngProblem(403,"text/html","<h1>Forbidden</h1>");
  const auto off=ac::checkSearxng(web,{true,false,403,0,forbidden.detail,"",forbidden.fix});
  check(!off.ok&&off.state=="JSON off"&&off.fix==forbidden.fix,"JSON off doctor check: "+off.fix);

  check(ac::searxngConfigUrl("http://127.0.0.1:8888/search")=="http://127.0.0.1:8888/config",
    "Config URL beside /search: "+ac::searxngConfigUrl("http://127.0.0.1:8888/search"));
  check(ac::searxngConfigUrl("https://search.example.org/searxng/search?language=en#x")==
    "https://search.example.org/searxng/config","Config URL under a prefix: "+
    ac::searxngConfigUrl("https://search.example.org/searxng/search?language=en#x"));
  check(ac::searxngConfigUrl("http://127.0.0.1:8888/")=="http://127.0.0.1:8888/config",
    "Config URL beside the homepage");
  check(ac::searxngConfigUrl("not a url").empty(),"A broken URL produced a config URL");
  const auto closed="http://127.0.0.1:"+std::to_string(closedPort())+"/search";
  const auto started=std::chrono::steady_clock::now();
  check(ac::searxngVersion(closed).empty(),"A closed port produced a version");
  check(std::chrono::steady_clock::now()-started<std::chrono::seconds(3),
    "The version probe was not time-boxed");
}

void missingScrapling(){
  Scrapling missing({"/nonexistent/batbox-test/scrapling","mcp"});
  check(!missing.running(),"A missing Scrapling was reported running");
  const auto first=thrown([&]{missing.listGadgets();});
  check(contains(first,"not installed")&&contains(first,"batbox setup web"),
    "Missing Scrapling error lacks the fix: "+first);
  check(contains(thrown([&]{missing.callGadget("get",Json::object());}),
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
    fetchToolDiscovery(fixture);
    doctorHandshake(root,fixture);
    scraplingBanner(root,fixture);
    scraplingMcp2();
    searxngShape();
    searxngDoctor();
    missingScrapling();
    unreachableSearxng();
    discovery(root);
    availability(root,fixture);
    fs::remove_all(root);
    std::cout<<"lazy Scrapling, fetch tool discovery, doctor handshake, Scrapling stdout banners, "
      "mcp 2 breakage, SearXNG answer shape, SearXNG doctor lines, unavailable web Gadgets, "
      "discovery and availability passed\n";
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
