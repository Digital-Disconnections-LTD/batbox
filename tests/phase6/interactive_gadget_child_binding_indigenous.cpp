#include "interactive/cards_gadget_binding/GadgetChildBinding.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
int main(){
  gadgets::Gadget gadget;gadget.name="Bash";
  gadget.renderGadgetResultMessage=[](const Json&,const Json&,const Json&){return Json();};
  gadget.renderGadgetUseRejectedMessage=[](const Json&,const Json&){return Json();};
  gadgets::Gadgets available{&gadget};
  interactive::MessageLookups lookups;
  auto owner=std::make_shared<Json>(Json{{"message",{{"content",Json::array({
    Json{{"type","tool_use"},{"id","id"},{"name","Bash"},
      {"input",{{"command","pwd"}}}}})}}}});
  lookups.gadgetUseByGadgetUseID.emplace("id",interactive::GadgetBlockRef{owner,0});
  interactive::cards_gadget_binding::RetainedSuccessStates retained;
  int approvalReads=0,deletions=0,rejectRenders=0;
  interactive::cards_gadget_binding::GadgetChildBindingInputs input;
  input.stableCardKey="stable-1";
  input.userMessage={{"toolUseResult",{{"ok",true}}}};
  input.progressMessages=Json::array();
  input.lookups=&lookups;input.gadgets=&available;input.terminalColumns=40;
  interactive::cards_gadget_binding::GadgetChildBindingServices services;
  services.success.classifierApprovalFeature=true;
  services.success.readTheme=[](){return Json("theme");};
  services.success.getClassifierApproval=[&](const std::string&){
    ++approvalReads;return std::optional<std::string>("rule");};
  services.success.getYoloClassifierApproval=[](const std::string&){
    return std::optional<std::string>{};};
  services.success.deleteClassifierApproval=[&](const std::string&){++deletions;};
  bool nullSuccess=false;
  services.success.renderGadgetResult=[&](const gadgets::Gadget&,const Json&,
    const Json&,const interactive::cards_gadget_children::SuccessRenderContext&){
    return nullSuccess?Json():Json("RESULT");};
  services.success.userFacingName=[](const gadgets::Gadget&){return std::string("Bash");};
  services.successProjection.subtle=ftxui::Color::GrayDark;
  services.successProjection.success=ftxui::Color::GreenLight;
  services.successProjection.tickGlyph="✓";
  services.successProjection.renderGadgetMessage=[](const Json& value){
    return ftxui::text(value.get<std::string>());};
  services.rejection.readColumns=[](){return 40;};
  services.rejection.readTheme=[](){return Json("theme");};
  services.rejection.safeParseInput=[](const gadgets::Gadget&,
    const Json& value)->std::optional<Json>{return value;};
  services.rejection.renderRejected=[&](const gadgets::Gadget&,const Json&,
    const interactive::cards_gadget_children::RejectRenderContext&){
    ++rejectRenders;return Json("REJECTED");};
  services.renderRejectedComponent=[](const Json& value){
    return ftxui::text(value.get<std::string>());};
  auto binding=interactive::cards_gadget_binding::bindGadgetResultChildren(
    retained,input,services,{});
  interactive::cards_gadget_result::GadgetResultCard route{
    interactive::cards_gadget_result::GadgetResultKind::Success,&gadget,
    lookups.gadgetUseByGadgetUseID.at("id"),"id"};
  auto card=binding.imports.renderSuccess(route,Json::object());
  assert(card);
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                    ftxui::Dimension::Fixed(2));
  ftxui::Render(screen,*card);
  assert(screen.ToString().find("RESULT")!=std::string::npos);
  binding.committed();
  assert(approvalReads==1&&deletions==1&&retained.size()==1);
  binding=interactive::cards_gadget_binding::bindGadgetResultChildren(
    retained,input,services,{});
  card=binding.imports.renderSuccess(route,Json::object());
  binding.committed();
  assert(card&&approvalReads==1&&deletions==1);
  input.stableCardKey="stable-2";nullSuccess=true;
  binding=interactive::cards_gadget_binding::bindGadgetResultChildren(
    retained,input,services,{});
  card=binding.imports.renderSuccess(route,Json::object());
  assert(!card);
  binding.committed();
  assert(approvalReads==2&&deletions==2);
  route.kind=interactive::cards_gadget_result::GadgetResultKind::Rejected;
  binding=interactive::cards_gadget_binding::bindGadgetResultChildren(
    retained,input,services,{});
  card=binding.imports.renderRejected(route,Json::array());
  assert(card&&rejectRenders==1);
  binding.committed();
  assert(deletions==2);
  retained.unmount("stable-1");
  assert(retained.size()==1);
  std::cout<<"4 native tool-child binding cases pass\n";
}
