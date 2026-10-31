#include "terminal_runtime_roster/HyperlinkSdk.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>

using namespace batbox;
int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});
  const auto cases=value::parse(input);
  Json rows=Json::array();
  for(const auto& test:cases){
    terminal_runtime_roster::DetectionContext context;
    context.stdoutIsTTY=test.value("tty",false);
    context.platform="linux";
    for(auto it=test.at("env").begin();it!=test.at("env").end();++it)
      context.env[it.key()]=it.value().get<std::string>();
    if(test.contains("argv"))
      for(const auto& arg:test.at("argv"))
        context.argv.push_back(arg.get<std::string>());
    const auto library=terminal_runtime_roster::createSupportsHyperlinks(context);
    auto liveEnv=std::make_shared<terminal_runtime_roster::Environment>(
      context.env);
    terminal_runtime_roster::HyperlinkSupport detector(context,
      [liveEnv](){return *liveEnv;});
    if(test.contains("mutateEnv")){
      liveEnv->clear();
      for(auto it=test.at("mutateEnv").begin();
          it!=test.at("mutateEnv").end();++it)
        (*liveEnv)[it.key()]=it.value().get<std::string>();
    }
    terminal_runtime_roster::SupportsHyperlinksOptions options;
    if(test.contains("options")){
      const auto& provided=test.at("options");
      if(provided.contains("stdoutSupported"))
        options.stdoutSupported=provided.at("stdoutSupported").get<bool>();
      if(provided.contains("env")){
        terminal_runtime_roster::Environment env;
        for(auto it=provided.at("env").begin();
            it!=provided.at("env").end();++it)
          env[it.key()]=it.value().get<std::string>();
        options.env=std::move(env);
      }
    }
    rows.push_back({{"name",test.at("name")},{"libraryStdout",library},
      {"result",detector.supportsHyperlinks(options)}});
  }
  std::cout<<rows.dump()<<'\n';
}
