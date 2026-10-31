#include "application_presentation/Bindings.hpp"
#include "interactive/message_view/RetainedViews.hpp"
#include <ftxui/screen/screen.hpp>
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
namespace ap=batbox::application_presentation;
namespace ip=batbox::interactive::presentation_providers;
namespace mv=batbox::interactive::message_view;
namespace {
std::string draw(const std::optional<ftxui::Element>& element){
  assert(element);auto screen=ftxui::Screen::Create(
    ftxui::Dimension::Fixed(72),ftxui::Dimension::Fixed(16));
  ftxui::Render(screen,*element);return screen.ToString();
}
}
int main(){
  event_loop::Loop loop;PresentationState state;state.viewport.columns=72;
  state.viewport.rows=16;
  terminal_ftxui::Options options;options.interactive=false;
  conversation_shell::Imports shell(loop,state,options);
  ip::ThemeState theme({[]{return std::string("dark");},
    [](const std::string&){},[]{return std::string("dark");},[]{}},"dark");
  messages::Services messages;
  messages.strictGadgetResultPairing=[]{return false;};
  messages.featureGate=[](const std::string&){return false;};
  messages.logError=[](const std::string&){assert(false);};
  gadgets::Gadget gadget;gadget.name="Read";
  gadget.parseInput=[](const Json& input){return input;};
  gadget.userFacingName=[](const Json&){return std::string("Read");};
  gadget.renderGadgetUseMessage=[](const Json&,const Json&){return Json("source.txt");};
  gadget.renderGadgetResultMessage=[](const Json& result,const Json&,const Json&){
    return Json("Read result: "+result.get<std::string>());
  };
  gadgets::Gadgets active{&gadget};
  ap::Inputs inputs;
  inputs.theme=&theme;inputs.messageServices=&messages;
  inputs.shortcuts.loadBindings=[]{return std::vector<ip::Binding>{};};
  inputs.gadgets=[&]{return active;};inputs.commands=[]{return Json::array();};
  inputs.modelLabel=[]{return std::string("local model");};
  inputs.workingDirectory=[]{return std::string("/tmp");};
  inputs.permissionMode=[]{return std::string("default");};
  inputs.statusText=[]{return std::string("ready");};
  auto bundle=ap::Bundle::bind(shell,std::move(inputs));
  auto user=std::make_shared<Json>(Json{{"type","user"},{"uuid","u"},
    {"message",{{"content",Json::array({{{"type","text"},{"text","real prompt"}}})}}}});
  auto assistant=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a"},
    {"message",{{"content",Json::array({{{"type","text"},{"text","**real answer**"}}})}}}});
  auto use=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","use"},
    {"message",{{"content",Json::array({{{"type","tool_use"},{"id","t"},
      {"name","Read"},{"input",Json::object()}}})}}}});
  auto result=std::make_shared<Json>(Json{{"type","user"},{"uuid","r"},
    {"toolUseResult","hello from file"},
    {"message",{{"content",Json::array({{{"type","tool_result"},
      {"tool_use_id","t"},{"content","hello from file"}}})}}}});
  auto lookups=std::make_shared<MessageLookups>();
  lookups->gadgetUseByGadgetUseID["t"]={use,0};
  lookups->resolvedGadgetUseIDs.insert("t");
  auto render=shell.projectionServices();render.lookups=lookups;
  mv::MessageOptions opts;opts.columns=72;opts.addMargin=false;
  auto imports=shell.messageImports();
  assert(draw(mv::projectMessage(mv::prepareMessage(RenderItem(user),opts,imports),render))
    .find("real prompt")!=std::string::npos);
  assert(draw(mv::projectMessage(mv::prepareMessage(RenderItem(assistant),opts,imports),render))
    .find("real answer")!=std::string::npos);
  auto retained=mv::RetainedViews::create(loop,[]{return 100.0;},[]{});
  retained->beginFrame("conversation");
  render=retained->bind(std::move(render),shell.retainedViews);
  assert(draw(mv::projectMessage(mv::prepareMessage(RenderItem(result),opts,imports),render))
    .find("Read result: hello from file")!=std::string::npos);
  retained->committed();
  std::cout<<"native application presentation workflow passed\n";
}
