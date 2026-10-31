#include "terminal_render/RenderToScreen.hpp"
#include "FakeFullLayout.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace tr=batbox::terminal_render;
namespace td=batbox::terminal_dom;
namespace {
struct Layout:FakeFullLayout {
  Json* events=nullptr;
  void setWidth(double w)override{events->push_back(Json::array({"setWidth",w}));}
  void calculateLayout(std::optional<double> w,std::optional<double>)override{
    events->push_back(Json::array({"layout",w?Json(*w):Json(nullptr)}));
  }
};
}
int main(){
  Json rows=Json::array(),events=Json::array();
  double tick=0;int renderCount=0;
  td::ElementRef firstRoot;
  tr::ReconcilerContainerRef firstContainer;
  tr::ScanState scan;scan.scanMs=5;
  tr::RenderToScreenImports imports;
  imports.createRoot=[&](){
    events.push_back(Json::array({"createRoot"}));
    auto root=std::make_shared<td::DOMElement>("ink-root");
    auto yoga=std::make_shared<Layout>();yoga->events=&events;yoga->height=2.2;
    root->yogaNode=yoga;firstRoot=root;return root;
  };
  imports.attachDisabledFocusManager=[&](td::DOMElement&){
    events.push_back(Json::array({"focus"}));
  };
  imports.createLegacyContainer=[&](td::DOMElement&){
    events.push_back(Json::array({"createContainer",7,"search-render"}));
    firstContainer=std::make_shared<int>(1);return firstContainer;
  };
  imports.updateContainerSync=[&](const tr::ReactElementRef& element,
      const tr::ReconcilerContainerRef& container){
    events.push_back(Json::array({"update",element?
      Json(*std::static_pointer_cast<int>(element)):Json(nullptr),
      container==firstContainer}));
  };
  imports.flushSyncWork=[&](){events.push_back(Json::array({"flush"}));};
  imports.performanceNow=[&](){events.push_back(Json::array({"now"}));
    return ++tick;};
  imports.resetLayoutShifted=[&](){events.push_back(Json::array({"reset"}));};
  imports.renderNodeToOutput=[&](td::DOMElement& root,
      terminal_output::Output&){
    events.push_back(Json::array({"render",&root==firstRoot.get(),true}));
    ++renderCount;
  };
  imports.flushOutput=[&](terminal_output::Output& output){
    events.push_back(Json::array({"get"}));return output.screen;
  };
  imports.logForDebugging=[&](const std::string& message){
    events.push_back(Json::array({"debug",message}));
  };
  tr::RenderToScreen renderer(imports,scan);
  for(int i=1;i<=21;i++){
    events=Json::array();
    auto result=renderer.render(std::make_shared<int>(i),30);
    rows.push_back(Json{{"call",i},{"events",events},
      {"height",result.height},{"screenWidth",result.screen->width},
      {"screenHeight",result.screen->height},
      {"renderCount",renderCount}});
  }
  std::cout<<rows.dump()<<'\n';
}
