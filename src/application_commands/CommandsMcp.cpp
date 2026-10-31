#include "application_commands/CommandsMcp.hpp"
#include "user_input/CommandMessages.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
std::vector<std::string> splitMcpArgs(const std::string& args){
  std::vector<std::string> result;
  std::size_t start=0;
  while(true){
    const auto end=args.find(' ',start);
    result.push_back(args.substr(start,end==std::string::npos?end:end-start));
    if(end==std::string::npos)break;
    start=end+1;
  }
  return result;
}
struct McpCommandFlight:std::enable_shared_from_this<McpCommandFlight>{
  using Done=interactive::CommandUi::Done;
  using Ready=interactive::CommandUi::Ready;
  using Error=interactive::CommandUi::Error;
  std::shared_ptr<mcp::manager::IndigenousRuntime> runtime;
  std::shared_ptr<McpAuthUi> auth;
  Done done;Ready ready;Error error;
  std::stop_source authStop;
  bool settled=false;
  std::string selected;
  void finish(std::string message){
    if(settled)return;
    settled=true;
    if(auth&&auth->server==selected){
      auth->changed={};auth->submitCallback={};auth->server.clear();
    }
    interactive::CommandCompletion result;
    result.result=std::move(message);
    result.options.display=interactive::DoneOptions::Display::System;
    done(std::move(result));
  }
  void fail(const std::string& message){
    if(settled)return;
    settled=true;
    if(auth&&auth->server==selected){
      auth->changed={};auth->submitCallback={};auth->server.clear();
    }
    error(message);
  }
  void progress(const std::string& title,const std::string& body){
    interactive::UiRequest request;
    request.kind=interactive::UiRequest::Kind::Progress;
    request.id="mcp-progress";request.title=title;request.body=body;
    auto self=shared_from_this();
    ready(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel){
        self->authStop.request_stop();self->finish("MCP action cancelled");
      }
    });
  }
  void reconnect(const std::string& name){
    selected=name;progress("MCP reconnect","Connecting to "+name+"...");
    try{
      auto self=shared_from_this();
      runtime->manager()->reconnect(name).then([self,name](
          const mcp::manager::ServerResult* result,
          const std::exception_ptr& failure){
        if(self->settled)return;
        if(failure){try{std::rethrow_exception(failure);}
          catch(const std::exception& e){self->fail(e.what());}
          catch(...){self->fail("MCP reconnect failed");}return;}
        if(!result){self->fail("MCP reconnect returned no server");return;}
        self->finish(name+": "+result->type+
          (result->error.empty()?std::string{}:" — "+result->error));
      });
    }catch(const std::exception& e){fail(e.what());}
  }
  void toggle(const std::string& name,bool enable){
    const auto servers=runtime->manager()->snapshot();
    std::vector<std::string> targets;
    for(const auto& server:servers){
      if(name!="all"&&server.name!=name)continue;
      if((server.type=="disabled")!=enable)continue;
      targets.push_back(server.name);
    }
    if(targets.empty()){
      finish(name=="all"?std::string("All MCP servers are already ")+
        (enable?"enabled":"disabled"):
        "MCP server not found or already "+std::string(enable?"enabled":"disabled"));
      return;
    }
    selected=name;progress("MCP servers",enable?"Enabling...":"Disabling...");
    auto self=shared_from_this();
    auto remaining=std::make_shared<std::size_t>(targets.size());
    const auto targetCount=targets.size();
    auto failures=std::make_shared<std::vector<std::string>>();
    for(const auto& target:targets){
      try{
        runtime->manager()->toggle(target).then([self,remaining,failures,
            target,enable,targetCount](const event_loop::Unit*,
              const std::exception_ptr& failure){
          if(failure)try{std::rethrow_exception(failure);}
            catch(const std::exception& e){failures->push_back(target+": "+e.what());}
            catch(...){failures->push_back(target+": unknown error");}
          if(--*remaining==0&&!self->settled){
            if(!failures->empty())self->fail(failures->front());
            else self->finish(std::string(enable?"Enabled ":"Disabled ")+
              std::to_string(targetCount)+" MCP server(s)");
          }
        });
      }catch(const std::exception& e){
        failures->push_back(target+": "+e.what());
        if(--*remaining==0&&!settled)fail(failures->front());
      }
    }
  }
  void logout(const std::string& name){
    selected=name;progress("MCP logout","Removing credentials for "+name+"...");
    try{
      auto self=shared_from_this();
      runtime->logout(name).then([self,name](const event_loop::Unit*,
          const std::exception_ptr& failure){
        if(self->settled)return;
        if(failure){try{std::rethrow_exception(failure);}
          catch(const std::exception& e){self->fail(e.what());}
          catch(...){self->fail("MCP logout failed");}return;}
        self->finish("Logged out of "+name);
      });
    }catch(const std::exception& e){fail(e.what());}
  }
  void showLogin(){
    if(settled||!auth)return;
    interactive::UiRequest request;
    request.kind=interactive::UiRequest::Kind::Form;
    request.id="mcp-login";request.title="Authorize "+selected;
    request.body=auth->url.empty()?"Preparing authorization URL...":
      "Open this URL:\n"+auth->url;
    request.body+="\n\nPaste the callback URL and press Enter, or wait for your browser to return.";
    if(!auth->callbackText.empty())
      request.body+="\nCallback: "+auth->callbackText;
    auto self=shared_from_this();
    ready(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel){
        self->authStop.request_stop();self->finish("MCP authorization cancelled");
      }else if(action.kind==interactive::UiAction::Kind::TextChange){
        if(action.text=="\x7f"||action.text=="\b"){
          if(!self->auth->callbackText.empty())self->auth->callbackText.pop_back();
        }else self->auth->callbackText+=action.text;
        self->showLogin();
      }else if(action.kind==interactive::UiAction::Kind::Activate&&
          self->auth->submitCallback&&!self->auth->callbackText.empty()){
        self->auth->submitCallback(self->auth->callbackText);
        self->auth->callbackText.clear();
        self->progress("MCP authorization","Exchanging authorization code...");
      }
    });
  }
  void login(const std::string& name){
    if(!auth){fail("MCP authorization UI is unavailable");return;}
    selected=name;auth->server=name;auth->url.clear();
    auth->callbackText.clear();auth->submitCallback={};
    const auto weak=weak_from_this();
    auth->changed=[weak]{if(auto self=weak.lock())self->showLogin();};
    showLogin();
    try{
      auto self=shared_from_this();
      runtime->authenticate(name,authStop.get_token()).then([self,name](
          const event_loop::Unit*,const std::exception_ptr& failure){
        if(self->settled)return;
        if(failure){try{std::rethrow_exception(failure);}
          catch(const std::exception& e){self->fail(e.what());}
          catch(...){self->fail("MCP authorization failed");}return;}
        self->finish("Connected to "+name);
      });
    }catch(const std::exception& e){fail(e.what());}
  }
  void showServer(const std::string& name,std::size_t cursor=0){
    selected=name;
    const auto servers=runtime->manager()->snapshot();
    const auto found=std::find_if(servers.begin(),servers.end(),
      [&name](const auto& server){return server.name==name;});
    if(found==servers.end()){showList();return;}
    interactive::UiRequest request;
    request.kind=interactive::UiRequest::Kind::Select;
    request.id="mcp-server";request.title=name;
    request.body="Status: "+found->type;
    if(!found->error.empty())request.body+="\n"+found->error;
    request.body+="\nTools: "+std::to_string(found->gadgets.size());
    request.choices={"Reconnect",found->type=="disabled"?"Enable":"Disable",
      "Log in", "Log out", "Back"};
    request.selectedIndex=std::min(cursor,request.choices.size()-1);
    auto self=shared_from_this();
    ready(std::move(request),[self,name](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel){self->showList();return;}
      if(action.kind==interactive::UiAction::Kind::MoveSelection){
        self->showServer(name,action.index);return;}
      if(action.kind!=interactive::UiAction::Kind::Activate)return;
      switch(action.index){
        case 0:self->reconnect(name);break;
        case 1:{
          const auto servers=self->runtime->manager()->snapshot();
          const auto found=std::find_if(servers.begin(),servers.end(),
            [&name](const auto& server){return server.name==name;});
          self->toggle(name,found!=servers.end()&&found->type=="disabled");break;}
        case 2:self->login(name);break;
        case 3:self->logout(name);break;
        default:self->showList();break;
      }
    });
  }
  void showList(std::size_t cursor=0){
    const auto servers=runtime->manager()->snapshot();
    interactive::UiRequest request;
    request.kind=servers.empty()?interactive::UiRequest::Kind::Notice:
      interactive::UiRequest::Kind::Select;
    request.id="mcp";request.title="MCP servers";
    if(servers.empty())request.body="No MCP servers configured";
    else for(const auto& server:servers)
      request.choices.push_back(server.name+"  ["+server.type+"]");
    request.selectedIndex=servers.empty()?0:
      std::min(cursor,servers.size()-1);
    auto self=shared_from_this();
    ready(std::move(request),[self,servers](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel){
        self->finish("MCP dialog dismissed");return;}
      if(action.kind==interactive::UiAction::Kind::MoveSelection){
        self->showList(action.index);return;}
      if(action.kind==interactive::UiAction::Kind::Activate){
        if(servers.empty())self->finish("No MCP servers configured");
        else if(action.index<servers.size())self->showServer(servers[action.index].name);
      }
    });
  }
  void start(std::string args){
    args=value::trim(args);
    if(args.empty()||args=="list"||args=="no-redirect"){
      showList();return;}
    const auto space=args.find(' ');
    const auto action=args.substr(0,space);
    const auto name=space==std::string::npos?std::string{}:
      value::trim(args.substr(space+1));
    if(action=="reconnect"&&!name.empty()){reconnect(name);return;}
    if((action=="enable"||action=="disable")){
      toggle(name.empty()?"all":name,action=="enable");return;}
    if(action=="login"&&!name.empty()){login(name);return;}
    if(action=="logout"&&!name.empty()){logout(name);return;}
    finish("Use /mcp, /mcp reconnect NAME, /mcp enable|disable [NAME], "
      "/mcp login NAME, or /mcp logout NAME");
  }
};
}
user_input::CommandEntry makeMcpPoetryCommand(
    const std::shared_ptr<mcp::manager::IndigenousRuntime>& runtime,
    const mcp::ResultServices& results,
    const std::function<event_loop::Deferred<Json>(const std::string&)>& getAttachments,
    const std::shared_ptr<messages::Services>& messages,
    const std::shared_ptr<bootstrap::InvokedSkillsState>& invokedSkills,
    const std::function<std::shared_ptr<bats::BatContext>()>& currentBat,
    event_loop::Loop& owner,const Json& descriptor){
  user_input::CommandEntry entry;
  entry.descriptor=descriptor;
  const auto display=value::property(descriptor,"userFacingName");
  entry.userFacingName=[display,descriptor]{
    return display.is_string()?display.get<std::string>():
      value::property(descriptor,"name").get<std::string>();};
  entry.expandPoetry=[runtime,results,getAttachments,messages,invokedSkills,
      currentBat,&owner,descriptor](
      const std::string& args,const Json& preceding,const Json& images,
      const std::optional<std::string>& uuid){
    event_loop::Deferred<user_input::InputResult> output(owner);
    try{
      const auto server=text(value::property(descriptor,"serverName"));
      const auto poetry=text(value::property(descriptor,"promptName"));
      if(server.empty()||poetry.empty()||!messages)
        throw std::logic_error("MCP prompt descriptor is incomplete");
      Json arguments=Json::object();
      const auto values=splitMcpArgs(args);
      const auto& names=value::property(descriptor,"argNames");
      if(names.is_array())for(std::size_t i=0;i<names.size()&&i<values.size();++i)
        if(names[i].is_string())arguments[names[i].get<std::string>()]=values[i];
      const Json params={{"name",poetry},{"arguments",arguments}};
      runtime->manager()->invoke(server,"prompts/get",params).then(
        [output,results,getAttachments,messages,invokedSkills,currentBat,
            descriptor,args,images,preceding,uuid,server](
            const Json* response,const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          try{
            const auto& returned=value::property(*response,"messages");
            if(!returned.is_array())
              throw std::runtime_error("MCP prompts/get response has no messages");
            Json resultBlocks=Json::array();
            for(const auto& message:returned){
              const auto& content=value::property(message,"content");
              if(content.is_array()){
                for(const auto& item:content)
                  for(const auto& transformed:mcp::transformResultContent(
                      item,server,results))resultBlocks.push_back(transformed);
              }else for(const auto& transformed:mcp::transformResultContent(
                  content,server,results))resultBlocks.push_back(transformed);
            }
            if(!getAttachments)throw std::logic_error(
              "MCP prompt attachment provider is unavailable");
            std::string attachmentText;
            for(const auto& block:resultBlocks){
              if(value::property(block,"type")!="text")continue;
              const auto value=text(value::property(block,"text"));
              if(!attachmentText.empty())attachmentText+=' ';
              attachmentText+=value;
            }
            if(invokedSkills){
              std::optional<std::string> batId;
              if(currentBat)if(auto bat=currentBat()){
                const auto value=value::property(bat->query.data,"agentId");
                if(value.is_string())batId=value.get<std::string>();
              }
              std::string compactContent;
              for(const auto& block:resultBlocks){
                if(value::property(block,"type")!="text")continue;
                if(!compactContent.empty())compactContent+="\n\n";
                compactContent+=text(value::property(block,"text"));
              }
              const auto name=text(value::property(descriptor,"name"));
              bootstrap::addInvokedSkill(*invokedSkills,name,"mcp:"+name,
                compactContent,std::move(batId),bootstrap::indigenousClockMillis);
            }
            Json blocks=Json::array();
            if(images.is_array())for(const auto& image:images)blocks.push_back(image);
            if(preceding.is_array())for(const auto& item:preceding)blocks.push_back(item);
            for(const auto& item:resultBlocks)blocks.push_back(item);
            getAttachments(attachmentText).then(
              [output,messages,descriptor,args,uuid,blocks](
                  const Json* attachments,const std::exception_ptr& failure){
                if(failure){output.reject(failure);return;}
                try{
                  if(!attachments||!attachments->is_array())
                    throw std::runtime_error("MCP prompt attachments must be an array");
                  user_input::InputResult result;
                  const auto commandName=text(value::property(descriptor,"name"));
                  Json metadata={{"content",user_input::formatCommandInputTags(
                    commandName,args)}};
                  if(uuid)metadata["uuid"]=*uuid;
                  result.messages.push_back(messages::createUserMessage(
                    metadata,*messages));
                  result.messages.push_back(messages::createUserMessage(
                    Json{{"content",blocks},{"isMeta",true}},*messages));
                  for(const auto& attachment:*attachments)
                    result.messages.push_back(attachment);
                  result.messages.push_back(Json{{"type","attachment"},
                    {"attachment",{{"type","command_permissions"},
                      {"allowedTools",Json::array()}}},
                    {"uuid",messages->randomUUID()},
                    {"timestamp",messages->nowISO()}});
                  result.shouldQuery=true;
                  output.resolve(std::move(result));
                }catch(...){output.reject(std::current_exception());}
              });
          }catch(...){output.reject(std::current_exception());}
        });
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  return entry;
}
user_input::CommandEntry makeMcpCommand(
    const std::shared_ptr<user_input::UiCommandRouter>& router,
    const std::shared_ptr<mcp::manager::IndigenousRuntime>& runtime,
    const std::shared_ptr<McpAuthUi>& auth){
  Json descriptor={{"type","local-jsx"},{"name","mcp"},
    {"description","Manage MCP server connections"},
    {"argumentHint","[reconnect|enable|disable|login|logout] [server]"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [runtime,auth](const std::string& args){
      return [runtime,auth,args](auto done,auto ready,auto error){
        auto command=std::make_shared<McpCommandFlight>();
        command->runtime=runtime;command->auth=auth;
        command->done=std::move(done);command->ready=std::move(ready);
        command->error=std::move(error);command->start(args);
      };
    });
}
} // namespace batbox::application_commands
