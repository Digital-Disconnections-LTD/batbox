#include "connectors/Scrapling.hpp"
#include "connectors/Executable.hpp"
#include "mcp/StdioClient.hpp"
#include <algorithm>
namespace batbox {
namespace {
constexpr std::size_t maxToolPages=100;
std::string joined(const std::vector<std::string>& names,const std::string& separator){
  std::string text;
  for(const auto& name:names){if(!text.empty())text+=separator;text+=name;}
  return text;
}
bool takes(const Json& schema,const std::string& argument){
  const auto& properties=value::property(schema,"properties");
  return !properties.is_object()||properties.contains(argument);
}
} // namespace

std::string scraplingToolMissingMessage(const std::vector<std::string>& wanted,
    const std::vector<std::string>& offered,const std::optional<std::string>& version,bool withFix){
  return (version?"Scrapling "+*version:std::string("This Scrapling"))+
    " has no tool named "+joined(wanted," or ")+" (it offers: "+
    (offered.empty()?std::string("no tools"):joined(offered,", "))+
    "); Batbox needs Scrapling >= "+minimumScraplingVersion+
    (withFix?". Run: batbox setup web":"");
}
ScraplingToolMissing::ScraplingToolMissing(std::vector<std::string> wantedTools,
    std::vector<std::string> offeredTools,const std::optional<std::string>& version)
  :std::runtime_error(scraplingToolMissingMessage(wantedTools,offeredTools,version)),
   wanted(std::move(wantedTools)),offered(std::move(offeredTools)){}

std::vector<ScraplingTool> scraplingTools(const Json& list){
  const auto& entries=value::property(list,"tools");
  if(!entries.is_array())throw std::runtime_error("Scrapling tools/list returned no tool array");
  std::vector<ScraplingTool> result;
  for(const auto& entry:entries){
    const auto& name=value::property(entry,"name");
    if(!name.is_string()||name.get<std::string>().empty())continue;
    result.push_back({name.get<std::string>(),value::property(entry,"inputSchema")});
  }
  return result;
}
const ScraplingTool* chooseScraplingTool(const std::vector<ScraplingTool>& offered,
    const std::vector<std::string>& wanted){
  for(const auto& name:wanted)
    for(const auto& tool:offered)if(tool.name==name)return &tool;
  return nullptr;
}

Scrapling::Scrapling(const std::vector<std::string>& argv,std::chrono::milliseconds wait,
    std::function<std::optional<std::string>()> version)
  :command(argv),timeout(wait),packageVersion(std::move(version)){
  if(command.empty())throw std::invalid_argument("Set a Scrapling MCP server executable");
}
Scrapling::~Scrapling(){
  std::shared_ptr<mcp::StdioClient> current;
  {std::lock_guard lock(state);current=std::move(client);}
  if(current)current->shutdown();
}
void Scrapling::shutdown()noexcept{
  stopped=true;
  std::shared_ptr<mcp::StdioClient> current;
  {std::lock_guard lock(state);current=std::move(client);tools.reset();}
  if(current)current->shutdown();
}
std::shared_ptr<mcp::StdioClient> Scrapling::live()const{
  std::lock_guard lock(state);
  if(!client)return nullptr;
  try{client->ensureConnected();return client;}catch(...){return nullptr;}
}
bool Scrapling::running()const noexcept{
  try{return live()!=nullptr;}catch(...){return false;}
}
std::string Scrapling::stderrDiagnostic()const{
  std::lock_guard lock(state);
  return client?client->stderrDiagnostic():std::string();
}
std::shared_ptr<mcp::StdioClient> Scrapling::connection(std::stop_token signal){
  checkConnectorAbort(signal);
  if(stopped)throw std::runtime_error("Scrapling is shutting down");
  if(auto current=live())return current;
  std::unique_lock lock(starting,std::defer_lock);
  while(!lock.try_lock_for(std::chrono::milliseconds(50)))checkConnectorAbort(signal);
  checkConnectorAbort(signal);
  if(stopped)throw std::runtime_error("Scrapling is shutting down");
  if(auto current=live())return current;
  if(!connectors::findExecutable(command.front()))
    throw std::runtime_error("Scrapling is not installed: \""+command.front()+
      "\" was not found. Web fetch is unavailable; run: batbox setup web "
      "(or set scrapling_command in native-client.json)");
  std::shared_ptr<mcp::StdioClient> fresh;
  try{
    fresh=std::make_shared<mcp::StdioClient>(command,"scrapling",timeout,
      std::map<std::string,std::string>{},
      std::min<std::chrono::milliseconds>(timeout,std::chrono::seconds(60)),
      std::nullopt,mcp::ClientHandlers{},signal);
  }catch(const ConnectorAbortError&){throw;}
  catch(const std::exception& error){
    if(signal.stop_requested())throw ConnectorAbortError();
    throw std::runtime_error(std::string("Scrapling could not start: ")+error.what());
  }
  std::shared_ptr<mcp::StdioClient> previous;
  {std::lock_guard guard(state);
    if(!stopped){previous=std::move(client);client=fresh;tools.reset();}}
  if(previous)previous->shutdown();
  if(stopped){fresh->shutdown();throw std::runtime_error("Scrapling is shutting down");}
  return fresh;
}
Json Scrapling::listGadgets(std::stop_token signal){
  return connection(signal)->listGadgets(signal);
}
Json Scrapling::callGadget(const std::string& name,const Json& arguments,std::stop_token signal){
  return connection(signal)->callGadgetResult(name,arguments,signal);
}
std::vector<ScraplingTool> Scrapling::advertisedGadgets(std::stop_token signal){
  const auto current=connection(signal);
  {std::lock_guard lock(state);if(client==current&&tools)return *tools;}
  std::vector<ScraplingTool> found;
  Json params=Json::object();
  for(std::size_t page=0;;++page){
    if(page==maxToolPages)throw std::runtime_error("Scrapling tools/list did not finish paging");
    const auto list=current->invoke("tools/list",params,signal);
    const auto listed=scraplingTools(list);
    found.insert(found.end(),listed.begin(),listed.end());
    const auto& cursor=value::property(list,"nextCursor");
    if(!cursor.is_string()||cursor.get<std::string>().empty())break;
    params={{"cursor",cursor}};
  }
  {std::lock_guard lock(state);if(client==current)tools=found;}
  return found;
}
ScraplingTool Scrapling::fetchGadget(std::stop_token signal){
  const auto offered=advertisedGadgets(signal);
  if(const auto* tool=chooseScraplingTool(offered,scraplingPlainFetchTools())){
    if(!takes(tool->inputSchema,"url"))
      throw std::runtime_error("Scrapling's "+tool->name+" tool takes no url argument; "
        "Batbox needs Scrapling >= "+minimumScraplingVersion+". Run: batbox setup web");
    return *tool;
  }
  std::vector<std::string> names;
  for(const auto& tool:offered)names.push_back(tool.name);
  std::optional<std::string> version;
  if(packageVersion)try{version=packageVersion();}catch(...){}
  throw ScraplingToolMissing(scraplingPlainFetchTools(),std::move(names),version);
}
Json Scrapling::fetch(const std::string& url,std::stop_token signal){
  const auto tool=fetchGadget(signal);
  Json arguments={{"url",url}};
  if(takes(tool.inputSchema,"extraction_type"))arguments["extraction_type"]="markdown";
  return callGadget(tool.name,arguments,signal);
}
} // namespace batbox
