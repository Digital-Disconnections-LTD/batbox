#include "application_commands/ConnectorsCommand.hpp"
#include <stdexcept>

namespace batbox::application_commands {
namespace {
using application_connectors::HelperCheck;
interactive::CommandCompletion completion(std::optional<std::string> message){
  interactive::CommandCompletion result;
  result.result=std::move(message);
  result.options.display=message?interactive::DoneOptions::Display::System:
    interactive::DoneOptions::Display::Skip;
  return result;
}
std::string describe(const HelperCheck& check){
  std::string text=check.name+": "+check.state;
  if(!check.detail.empty())text+="\n  "+check.detail;
  if(!check.fix.empty())text+="\n  fix: "+check.fix;
  return text;
}
HelperChecks liveMcp(const std::shared_ptr<mcp::manager::IndigenousRuntime>& runtime){
  HelperChecks checks;
  if(!runtime)return checks;
  for(const auto& server:runtime->manager()->snapshot())
    checks.push_back(application_connectors::mcpServerCheck(server.name,server.type,server.error));
  return checks;
}
enum class Row {Check,SetUp,Close};

class ConnectorsDialog final:public std::enable_shared_from_this<ConnectorsDialog> {
public:
  ConnectorsDialog(HelperChecks web,HelperChecks mcp,interactive::CommandUi::Done done,
      interactive::CommandUi::Ready ready):web_(std::move(web)),mcp_(std::move(mcp)),
      done_(std::move(done)),ready_(std::move(ready)){
    for(std::size_t i=0;i<web_.size();++i)rows_.push_back({Row::Check,&web_[i]});
    for(std::size_t i=0;i<mcp_.size();++i)rows_.push_back({Row::Check,&mcp_[i]});
    if(mcp_.empty())rows_.push_back({Row::Check,nullptr});
    rows_.push_back({Row::SetUp,nullptr});
    rows_.push_back({Row::Close,nullptr});
  }
  void show(){
    auto request=connectorsRequest(web_,mcp_);
    request.selectedIndex=std::min(selected_,request.choices.size()-1);
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){self->act(action);});
  }
private:
  void act(const interactive::UiAction& action){
    using Kind=interactive::UiAction::Kind;
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){done_(completion(std::nullopt));return;}
    if(action.kind==Kind::MoveSelection){selected_=std::min(action.index,rows_.size()-1);show();return;}
    if(action.kind!=Kind::Activate)return;
    const auto& [kind,check]=rows_[std::min(action.index,rows_.size()-1)];
    if(kind==Row::Close){done_(completion(std::nullopt));return;}
    if(kind==Row::SetUp){done_(completion(setupWebInstructions()));return;}
    if(!check){done_(completion(
      "No MCP servers are configured. Add them to mcp.json in your Batbox configuration folder."));
      return;}
    done_(completion(describe(*check)));
  }
  HelperChecks web_,mcp_;
  interactive::CommandUi::Done done_;
  interactive::CommandUi::Ready ready_;
  std::vector<std::pair<Row,const HelperCheck*>> rows_;
  std::size_t selected_=0;
};
}

std::string setupWebInstructions(){
  return "Run this in a terminal (a few minutes; no Docker, no prompts):\n"
    "  batbox setup web\n"
    "It installs Scrapling with pipx or uv and SearXNG in its own virtual environment, "
    "running as a user service on 127.0.0.1:8888. Preview it with "
    "batbox setup web --dry-run; remove it with batbox setup web --uninstall. "
    "Restart Batbox afterwards.";
}

interactive::UiRequest connectorsRequest(const HelperChecks& web,const HelperChecks& mcp){
  interactive::UiRequest request;
  request.kind=interactive::UiRequest::Kind::Select;
  request.id="connectors";request.title="Connectors";
  request.body="Batbox works without these. Web Gadgets use them when present.";
  request.hint="↑↓ choose · enter details · esc close";
  const auto add=[&](const HelperCheck& check,const std::string& section){
    request.choices.push_back(check.name);
    interactive::UiChoice info;
    info.section=section;info.badge=check.state;
    // The fix comes first so a narrow dialog still shows it; enter shows all.
    info.detail=check.fix.empty()?check.detail:
      "fix: "+check.fix+(check.detail.empty()?"":" · "+check.detail);
    request.choiceInfo.push_back(std::move(info));
  };
  for(const auto& check:web)add(check,"Web helpers");
  for(const auto& check:mcp)add(check,"MCP servers");
  if(mcp.empty()){
    request.choices.push_back("No MCP servers");
    request.choiceInfo.push_back({"Add servers to mcp.json in your Batbox configuration folder",
      "",  "MCP servers"});
  }
  request.choices.push_back("Set up now");
  request.choiceInfo.push_back({"Shows the command: batbox setup web (Scrapling and a local SearXNG)",
    "","Actions"});
  request.choices.push_back("Close");
  request.choiceInfo.push_back({"","","Actions"});
  return request;
}

std::string connectorsText(const HelperChecks& web,const HelperChecks& mcp){
  return application_connectors::formatDoctor(web,mcp,{},"Connectors");
}

user_input::CommandEntry makeConnectorsCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,ConnectorsInputs inputs){
  if(!router)throw std::invalid_argument("Connectors command needs a router");
  if(!inputs.checkWeb)inputs.checkWeb=[config=inputs.config]{
    return application_connectors::checkWebHelpers(config);};
  if(!inputs.checkMcp)inputs.checkMcp=[runtime=inputs.mcpRuntime]{return liveMcp(runtime);};
  auto shared=std::make_shared<ConnectorsInputs>(std::move(inputs));
  Json descriptor={{"type","local-jsx"},{"name","connectors"},
    {"description","Check web helpers (Scrapling, SearXNG) and MCP servers"},
    {"supportsNonInteractive",true}};
  return user_input::bindUiCommand(std::move(descriptor),std::move(router),
    [shared,&owner](const std::string&){
      return [shared,&owner](auto done,auto ready,auto error){
        if(shared->interactive){
          interactive::UiRequest progress;
          progress.kind=interactive::UiRequest::Kind::Progress;
          progress.id="connectors-progress";progress.title="Connectors";
          progress.body="Checking Scrapling and SearXNG…";
          ready(std::move(progress),[done](const interactive::UiAction& action){
            if(action.kind==interactive::UiAction::Kind::Cancel)done(completion(std::nullopt));
          });
        }
        auto web=std::make_shared<HelperChecks>();
        owner.queueWork([shared,web]{*web=shared->checkWeb();},
          [shared,web,done,ready,error](std::exception_ptr failure){
            if(failure){
              try{std::rethrow_exception(failure);}
              catch(const std::exception& issue){error(issue.what());}
              catch(...){error("Connector checks failed");}
              return;
            }
            const auto mcp=shared->checkMcp();
            if(!shared->interactive){done(completion(connectorsText(*web,mcp)));return;}
            std::make_shared<ConnectorsDialog>(*web,mcp,done,ready)->show();
          });
      };
    });
}
} // namespace batbox::application_commands
