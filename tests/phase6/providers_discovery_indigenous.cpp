// Probe driver for providers_fake_servers.py: each argument is preset:port.
// Prints the discovered servers as JSON plus the wall time of the probe.
#include "providers/LocalDiscovery.hpp"
#include <chrono>
#include <iostream>

int main(int argc,char** argv){
  using namespace batbox;
  std::vector<providers::ProbeTarget> targets;
  for(int i=1;i<argc;++i){
    const std::string argument=argv[i];
    const auto colon=argument.rfind(':');
    if(colon==std::string::npos){std::cerr<<"expected preset:port\n";return 2;}
    providers::ProbeTarget target;
    target.preset=argument.substr(0,colon);target.name=target.preset;
    target.port=std::stoi(argument.substr(colon+1));
    targets.push_back(target);
  }
  const auto started=std::chrono::steady_clock::now();
  const auto servers=providers::discoverLocalServers(targets);
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now()-started).count();
  Json rows=Json::array();
  for(const auto& server:servers){
    Json contexts=Json::object();
    for(const auto& [id,context]:server.contexts)contexts[id]=context;
    rows.push_back({{"preset",server.preset},{"name",server.name},{"port",server.port},
      {"base_url",server.baseUrl},{"models",server.models},{"contexts",contexts},
      {"confirmed",server.confirmed},{"summary",providers::describeLocalServer(server)}});
  }
  std::cout<<Json{{"elapsed_ms",elapsed},{"servers",rows}}.dump()<<"\n";
}
