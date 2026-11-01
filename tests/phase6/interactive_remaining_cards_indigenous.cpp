#include "interactive/cards_remaining/AttachmentProjection.hpp"
#include "interactive/cards_remaining/MessageLeavesProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive::cards_remaining;
namespace {
std::string render(ftxui::Element element,int rows=4){
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(70),
                                    ftxui::Dimension::Fixed(rows));
  ftxui::Render(screen,element);return screen.ToString();
}
}
int main(){
  LeafPalette palette;palette.blackCircle="●";palette.successTick="✓";
  palette.subtle=ftxui::Color::GrayDark;
  palette.isBlinking=[](bool){return true;};
  ImageServices imageServices;
  imageServices.getStoredImagePath=[](int){return std::optional<std::string>("/tmp/p.png");};
  imageServices.supportsHyperlinks=[](){return true;};
  imageServices.pathToFileURL=[](const std::string&){return "file:///tmp/p.png";};
  const auto image=classifyUserImage(2,false,imageServices);
  assert(image.fileURL&&image.label=="[Image #2]");
  assert(render(projectUserImage(image,palette)).find("Image #2")!=std::string::npos);
  CompactServices compactServices;
  compactServices.getUserMessageText=[](const Json&){return "SUMMARY BODY";};
  compactServices.shortcutHint=[](const std::string& description){
    return "(ctrl+o to "+description+")";};
  const auto compact=classifyCompactSummary(Json{{"summarizeMetadata",{
    {"messagesSummarized",5},{"direction","up_to"},{"userContext","focus"}}}},
    "repl",compactServices);
  assert(compact.metadata&&compact.body=="Summarized 5 messages up to this point");
  assert(render(projectCompactSummary(compact,palette),5).find(
    "Summarized conversation")!=std::string::npos);
  assert(compactBoundaryText([]{return "Ctrl+O";})==
    "✻ Conversation compacted (Ctrl+O for history)");
  AdvisorServices advisorServices;
  advisorServices.renderModelName=[](const std::string& value){return value;};
  advisorServices.expandHint=[](){return "(Ctrl+O to expand)";};
  interactive::GadgetIDs resolved,errored;
  const auto advisor=classifyAdvisor(Json{{"type","server_tool_use"},
    {"id","a"},{"input",{{"topic","safety"}}}},true,resolved,errored,
    true,false,std::optional<std::string>("local model"),advisorServices);
  assert(advisor.kind==AdvisorCard::Kind::GadgetUse);
  assert(render(*projectAdvisor(advisor,palette)).find("Advising")!=std::string::npos);
  const auto reviewed=classifyAdvisor(Json{{"content",{
    {"type","advisor_result"},{"text","detail"}}}},false,resolved,
    errored,false,false,{},advisorServices);
  assert(render(*projectAdvisor(reviewed,palette)).find("Advisor has reviewed")!=
    std::string::npos);
  AttachmentServices attachmentServices;
  attachmentServices.readSelectedBackground=[](){return Json();};
  attachmentServices.skillSearchEnabled=[](){return false;};
  attachmentServices.batSwarmsEnabled=[](){return true;};
  attachmentServices.isShutdownApproved=[](const std::string& text){
    return text=="approved";};
  attachmentServices.isPlanApproval=[](const std::string&,const std::string&){
    return false;};
  attachmentServices.formatPartnerContent=[](const std::string&)
      ->std::optional<std::string>{return {};};
  attachmentServices.lookupMission=[](const std::string&)->std::optional<Json>{return {};};
  AttachmentProjectionImports projection;
  projection.palette.blackCircle="●";
  projection.palette.subtle=ftxui::Color::GrayDark;
  projection.renderUserText=[](const std::string& value,bool,bool,bool){
    return ftxui::text(value);};
  projection.renderImage=[](int id){return ftxui::text("image "+std::to_string(id));};
  projection.renderDiagnostics=[](const Json&,bool){return ftxui::text("diagnostics");};
  bool sawTranscriptMode=false;
  projection.renderPartnerText=[&sawTranscriptMode](const MailboxItem& item,
      bool transcriptMode){
    sawTranscriptMode=transcriptMode;
    return ftxui::text(item.text);};
  auto attachment=classifyAttachment(Json{{"type","queued_command"},
    {"prompt","queued text"},{"imagePasteIds",Json::array({2})}},
    false,false,false,attachmentServices);
  assert(attachment.kind==AttachmentCard::Kind::UserPoetry);
  assert(render(*projectAttachment(attachment,projection)).find("queued text")!=
    std::string::npos);
  attachment=classifyAttachment(Json{{"type","teammate_mailbox"},
    {"messages",Json::array({Json{{"text","approved"}},
      Json{{"text","{\"type\":\"idle_notification\"}"}},
      Json{{"text","hello"},{"from","alice"}}})}},
    false,false,false,attachmentServices);
  assert(attachment.kind==AttachmentCard::Kind::Mailbox&&
         attachment.mailbox.size()==1);
  assert(render(*projectAttachment(attachment,projection)).find("hello")!=
         std::string::npos);
  attachment=classifyAttachment(Json{{"type","teammate_mailbox"},
    {"messages",Json::array({Json{{"text","hello"},{"from","alice"}}})}},
    false,false,true,attachmentServices);
  (void)projectAttachment(attachment,projection);
  assert(sawTranscriptMode);
  attachment=classifyAttachment(Json{{"type","task_status"},{"taskId","t1"},
    {"taskType","local"},{"status","completed"},{"description","build"}},
    false,false,false,attachmentServices);
  assert(attachment.kind==AttachmentCard::Kind::Lines);
  {
    std::string text;
    for(const auto& line:attachment.lines)for(const auto& span:line.spans)text+=span.text;
    assert(text=="● Mission \"build\" completed in background");
    assert(render(*projectAttachment(attachment,projection)).find("Mission")!=std::string::npos);
  }
  std::cout<<"9 native remaining-card composition cases pass\n";
}
