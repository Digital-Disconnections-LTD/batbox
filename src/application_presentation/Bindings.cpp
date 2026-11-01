#include "application_presentation/Bindings.hpp"
#include "interactive/presentation_providers/CardPalettes.hpp"
#include "interactive/presentation_providers/ThemeData.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "interactive/markdown/Highlight.hpp"
#include "interactive/cards_gadget_result/GadgetErrorProjection.hpp"
#include "interactive/cards_remaining/MessageLeaves.hpp"
#include "interactive/cards_remaining/AttachmentProjection.hpp"
#include "interactive/text_input/MeasuredText.hpp"
#include "npm_wrap_ansi/Wrap.hpp"
#include "runtime/Encoding.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace batbox::application_presentation {
namespace ip=batbox::interactive::presentation_providers;
namespace {
std::string field(const Json& value,const char* name){
  if(!value.is_object())return {};
  auto it=value.find(name);
  return it!=value.end()&&it->is_string()?it->get<std::string>():std::string{};
}
namespace gv=batbox::interactive::gadget_views;
// Plain rendering for values outside the Gadget card path (system notices,
// attachments): terminal-safe text, never a raw JSON dump of a known field.
ftxui::Element renderComponent(const Json& value){
  return ftxui::paragraph(value::encodeUtf8(gv::sanitize(gv::visibleText(value))));
}
// Parallel Bats arrive as "N Bats" followed by one indented line per Bat.
Json groupView(const Json& value,bool running){
  if(!value.is_string())return value;
  const auto lines=gv::splitLines(value.get<std::string>());
  if(lines.empty())return value;
  Json entries=Json::array();
  for(std::size_t i=1;i<lines.size();++i)entries.push_back(value::trim(lines[i]));
  return Json{{gv::viewKey,"group"},{"title",lines.front()},{"entries",entries},
    {"running",running}};
}
double steadyMs(){
  return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count());
}
std::string seconds(double milliseconds){
  if(!(milliseconds>=0))return "0s";
  if(milliseconds<1000)return std::to_string(static_cast<int>(milliseconds))+"ms";
  const auto tenths=static_cast<long long>(milliseconds/100.0);
  return std::to_string(tenths/10)+"."+std::to_string(tenths%10)+"s";
}
std::optional<Json> parseInput(const gadgets::Gadget& gadget,const Json& input){
  try{return gadget.parseInput?gadget.parseInput(input):input;}catch(...){return {};}
}
std::string facingName(const gadgets::Gadget& gadget,const std::optional<Json>& input){
  return gadget.userFacingName?gadget.userFacingName(input.value_or(Json::object())):gadget.name;
}
Json gadgetTheme(const ip::ThemeState& theme){return Json{{"name",theme.currentTheme()}};}
ftxui::Color resolve(const Json& value){
  return value.is_string()?ip::parseColor(value.get<std::string>()).value_or(ftxui::Color::Default):ftxui::Color::Default;
}
std::string basename(const std::string& path){return std::filesystem::path(path).filename().string();}
std::string fileUrl(const std::string& path){return "file://"+path;}
std::string userText(const Json& message){
  if(message.contains("message")&&message["message"].is_object()){
    auto content=message["message"].value("content",Json());
    if(content.is_string())return content.get<std::string>();
    if(content.is_array())for(const auto& part:content){auto text=field(part,"text");if(!text.empty())return text;}
  }
  return gv::visibleText(message.value("content",Json()));
}
}
struct Bundle::MarkdownPalette {
  std::string theme;
  bool colors=true;
  interactive::markdown::FormatServices::BlockStyle blocks;
  std::string heading,headingGlyph,headingMinorGlyph,link,inlineCode,listMarker,
    strongLeadIn,taskDone,taskOpen;
  interactive::markdown::SyntaxPalette syntax;
};
void Bundle::refreshMarkdownPalette()const{
  const auto theme=inputs_.theme->currentTheme();
  auto& palette=*markdownPalette_;
  if(palette.theme==theme)return;
  const auto& roles=ip::styleRoles(theme);
  const auto sgr=[&](const ftxui::Color& color,bool background=false){
    return palette.colors?ip::sgr(color,background):std::string{};};
  palette.theme=theme;
  palette.heading=sgr(roles.heading);
  palette.headingGlyph=sgr(roles.headingGlyph);
  palette.headingMinorGlyph=sgr(roles.headingMinorGlyph);
  palette.link=sgr(roles.link);
  palette.inlineCode=sgr(roles.inlineCode)+sgr(roles.inlineCodeBg,true);
  palette.listMarker=sgr(roles.listMarker);
  palette.strongLeadIn=sgr(roles.lantern);
  palette.taskDone=sgr(roles.statusOk);
  palette.taskOpen=sgr(roles.muted);
  palette.blocks.codeFrame=sgr(roles.codeFrame);
  palette.blocks.codeLabel=sgr(roles.codeLabel);
  palette.blocks.codeBackground=sgr(roles.codeBg,true);
  palette.blocks.rule=sgr(roles.faint);
  palette.blocks.listMarker=palette.listMarker;
  palette.blocks.readingWidth=100;
  palette.syntax={sgr(roles.syntaxKeyword),sgr(roles.syntaxString),
    sgr(roles.syntaxNumber),sgr(roles.syntaxComment),sgr(roles.syntaxType),
    sgr(roles.diffAdd),sgr(roles.diffDel),sgr(roles.diffHunk)};
}
std::string Bundle::spinnerGlyph()const{
  static const char* frames[]={"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
  const auto now=inputs_.nowMs?inputs_.nowMs():steadyMs();
  return frames[static_cast<std::size_t>(std::max(0.0,now)/100.0)%10];
}
std::string Bundle::expandKey()const{
  return inputs_.shortcuts.display("expand","global","ctrl+o");
}
Bundle::Bundle(interactive::conversation_shell::Imports& shell,Inputs inputs):
    inputs_(std::move(inputs)),state_(&shell.presentation){
  if(!inputs_.theme||!inputs_.messageServices||!inputs_.gadgets||!inputs_.commands||
     !inputs_.modelLabel||!inputs_.workingDirectory||!inputs_.permissionMode||
     !inputs_.statusText||!inputs_.shortcuts.loadBindings)
    throw std::invalid_argument("Indigenous presentation requires live theme, messages, Roster, commands, and chrome selectors");
  if(!inputs_.feature)inputs_.feature=[](const std::string&){return false;};
  if(!inputs_.log)inputs_.log=[](const std::string&){};
  if(!inputs_.displayPath)inputs_.displayPath=[](const std::string& path){return path;};
  intl_=std::make_shared<intl::Stash>(intl::indigenousServices());
  width_=std::make_shared<text_width::StringWidth>(text_width::indigenousServices(intl_));
  wrap_=text_width::indigenousWrapAnsiModule(false).wrapAnsi();
  markdownPalette_=std::make_shared<MarkdownPalette>();
  markdownPalette_->colors=inputs_.colors;
  refreshMarkdownPalette();
  const auto palette=markdownPalette_;
  interactive::markdown::SyntaxHighlighter highlighter{
    [](const std::string& language){return interactive::markdown::supportsSyntaxLanguage(language);},
    [palette](const std::string& code,const std::string& language){
      return interactive::markdown::highlightSyntax(code,language,palette->syntax);},
    inputs_.log};
  auto customize=[palette](interactive::markdown::FormatServices& services){
    const auto wrap=[](const std::string& sgr,const std::string& text,const char* reset){
      return sgr.empty()?text:sgr+text+reset;};
    services.heading=[palette,wrap,bold=services.bold](const std::string& content,int depth){
      const auto& p=*palette;
      const auto title=bold(wrap(p.heading,content,"\x1b[39m"));
      if(depth==1)return wrap(p.headingGlyph,"✦ ","\x1b[39m")+title;
      if(depth==2)return wrap(p.headingMinorGlyph,"◆ ","\x1b[39m")+title;
      return title;
    };
    services.link=[palette,wrap](const std::string& text){
      return palette->colors?wrap(palette->link+"\x1b[4m",text,"\x1b[24;39m"):text;};
    services.inlineCode=[palette,wrap](const std::string& text){
      return wrap(palette->inlineCode,text,"\x1b[39;49m");};
    services.listMarker=[palette,wrap](const std::string& prefix){
      return wrap(palette->listMarker,prefix=="- "?std::string("• "):prefix,"\x1b[39m");};
    services.strongLeadIn=[palette,wrap](const std::string& text){
      return wrap(palette->strongLeadIn,text,"\x1b[39m");};
    services.taskMarker=[palette,wrap](bool checked){
      return checked?wrap(palette->taskDone,"[✓] ","\x1b[39m"):
        wrap(palette->taskOpen,"[ ] ","\x1b[39m");};
    services.blockStyle=[palette]{return palette->blocks;};
  };
  markdown_=interactive::markdown::makeIndigenousRenderer(width_,
    state_->viewport.columns,inputs_.colors,inputs_.hyperlinks,"",
    std::move(highlighter),std::nullopt,std::move(customize));
}
std::shared_ptr<Bundle> Bundle::bind(interactive::conversation_shell::Imports& shell,Inputs inputs){
  auto self=std::shared_ptr<Bundle>(new Bundle(shell,std::move(inputs)));
  shell.listSettings=[self](const auto& state){return self->listSettings(state);};
  shell.surfaceOptions=[self](const auto& state){return self->surfaceOptions(state);};
  shell.messageImports=[self]{
    interactive::message_view::MessageImports out;
    out.fullscreen=[]{return true;};out.feature=self->inputs_.feature;
    out.isSnipBoundary=[](const Json& value){return value.value("type","")=="snip_boundary";};
    out.isSnipMarker=[](const Json& value){return value.value("type","")=="snip_marker";};
    out.isConnectorTextBlock=[](const Json& value){return value.value("type","")=="connector_text";};
    out.isAdvisorBlock=[](const Json& value){return value.value("type","")=="server_tool_use";};
    out.logError=self->inputs_.log;return out;
  };
  shell.decisionImports=[self]{
    interactive::MessageDecisionImports out;
    out.isCollapsibleGadget=[self](const std::string& name,const Json& input){
      const auto gadgets=self->inputs_.gadgets();const auto* gadget=gadgets::findGadgetByName(gadgets,name);
      if(!gadget||!gadget->isSearchOrReadCommand)return false;
      auto result=gadget->isSearchOrReadCommand(input);
      return result.is_object()&&(result.value("isSearch",false)||result.value("isRead",false)||result.value("isList",false));
    };return out;
  };
  shell.projectionServices=[self]{return self->projectionServices();};
  shell.retainedViews=self->retainedServices();
  shell.renderStreamingMarkdown=[self](const std::string& text,bool dim){
    self->refreshMarkdownPalette();
    self->markdown_->setTerminalColumns(self->state_->viewport.columns);
    return self->markdown_->renderStreaming(self->streaming_,text,dim);
  };
  shell.liveRetainedViews=[self]{return self->retainedServices();};
  shell.chrome=ip::makeChromeProviders({*self->inputs_.theme,self->inputs_.shortcuts,
    self->inputs_.modelLabel,self->inputs_.workingDirectory,
    self->inputs_.permissionMode,self->inputs_.statusText,
    self->inputs_.gitBranch,self->inputs_.contextUsage,self->inputs_.homeDirectory});
  shell.editorMetrics=interactive::text_input::indigenousMetricsServices(self->intl_,self->width_,self->wrap_);
  shell.editorOptions.columns=self->state_->viewport.columns;
  return self;
}
interactive::MessageListSettings Bundle::listSettings(const interactive::PresentationState&)const{
  interactive::MessageListSettings out;
  out.messageServices=inputs_.messageServices;
  out.gadgets=std::make_shared<interactive::GroupingGadgetSet>();
  const auto active=inputs_.gadgets();out.readSearch.gadgets=active;
  for(const auto* gadget:active)if(gadget){
    out.gadgets->gadgets.push_back({gadget->name,static_cast<bool>(gadget->renderGroupedGadgetUse)});
  }
  out.visibility={"(no content)","[Request interrupted by user for Gadget use]",inputs_.feature};
  auto& cls=out.readSearch.classification;
  cls.feature=inputs_.feature;cls.isFullscreen=[]{return true;};
  cls.isAutoManagedMemoryFile=[](const std::string&){return false;};
  cls.isMemoryDirectory=[](const std::string&){return false;};
  cls.isAutoManagedMemoryPattern=[](const std::string&){return false;};
  cls.isShellCommandTargetingMemory=[](const std::string&){return false;};
  cls.getReplPrimitiveGadgets=[active]{return active;};
  cls.replGadgetName="REPL";cls.writeGadgetName="Write";cls.editGadgetName="Edit";
  cls.gadgetSearchName="ToolSearch";cls.bashGadgetName="Bash";
  out.readSearch.extractBashCommentLabel=[](const std::string& command)->std::optional<std::string>{
    auto pos=command.find('#');if(pos==std::string::npos)return {};
    auto end=command.find('\n',pos);return command.substr(pos+1,end==std::string::npos?end:end-pos-1);
  };
  out.readSearch.getDisplayPath=inputs_.displayPath;
  out.readSearch.isTeamMemoryWriteOrEdit=[](const std::string&,const Json&){return false;};
  out.readSearch.isTeamMemorySearch=[](const Json&){return false;};
  out.readSearch.isTeamMemFile=[](const std::string&){return false;};
  out.readSearch.detectGitOperation=[](const std::string&,const std::string&){return Json();};
  return out;
}
interactive::message_list_view::SurfaceOptions Bundle::surfaceOptions(const interactive::PresentationState& state)const{
  interactive::message_list_view::SurfaceOptions out;
  out.screen="prompt";out.conversationId=state.conversationId;
  out.columns=state.viewport.columns;out.virtualScroll=true;
  out.isLoading=state.loading;out.streamingText=state.streamingText;
  out.thinkingInPlace=true;
  return out;
}
interactive::message_view::ProjectionServices Bundle::projectionServices()const{
  using namespace interactive;
  message_view::ProjectionServices out;
  const auto theme=inputs_.theme->currentTheme();
  const auto& roles=ip::styleRoles(theme);
  refreshMarkdownPalette();
  const auto subtle=roles.muted;
  const auto success=roles.gadgetOk;
  const auto error=roles.gadgetErr;
  const int columns=std::max(1,state_->viewport.columns);
  const auto viewStyle=ip::gadgetViewStyle(roles,expandKey());
  out.gadgets=inputs_.gadgets();out.commands=inputs_.commands();
  out.user.protocol={"(no content)","[Request interrupted by user]",
    "[Request interrupted by user for Gadget use]","command-message",
    "local-command-caveat","tick","teammate-message","task-notification",
    "GITHUB","FORK","INBOX","CHANNEL","CHANNELS",inputs_.feature,
    [this]{return inputs_.feature("AGENT_SWARMS");}};
  out.user.userPalette=ip::userCardPalette(*inputs_.theme);
  out.user.poetryPalette=ip::poetryPalette(*inputs_.theme);
  out.user.poetry.logError=inputs_.log;
  out.user.poetry.briefModeActive=[]{return false;};
  out.user.poetry.userMessageOptIn=[]{return false;};
  out.user.poetry.briefPreference=[]{return false;};
  // Sent poetry keeps the input's prompt glyph.
  out.user.highlight.pointerGlyph="❯";
  out.user.bashResult.outputPalette=ip::outputLinePalette(*inputs_.theme);
  out.user.bashResult.dimColor=subtle;
  out.user.bashResult.outputLine.stringWidth=[width=width_](const std::string& text){return width->stringWidth(Json(text));};
  out.user.bashResult.outputLine.sliceAnsi=[](const std::string& text,double start,double end){
    const auto begin=static_cast<std::size_t>(std::max(0.0,start));
    const auto length=static_cast<std::size_t>(std::max(0.0,end-start));
    return text.substr(std::min(begin,text.size()),length);
  };
  out.user.bashResult.outputLine.dim=[](const std::string& text){return text;};
  out.user.bashResult.outputLine.expandHint=[this]{return inputs_.shortcuts.display("expand","global","ctrl+o");};
  out.user.bashResult.backgroundShortcutHint=[this]{return inputs_.shortcuts.display("background","global","ctrl+b");};
  out.user.renderOtherCard=[viewStyle,columns](cards::UserTextKind kind,const Json&,
      const cards::UserTextCard& card){
    if(kind==cards::UserTextKind::LocalCommandOutput){
      // Slash-command output hangs under the command like Gadget output.
      const auto between=[&](const std::string& tag){
        const auto open="<"+tag+">",close="</"+tag+">";
        const auto start=card.text.find(open);
        if(start==std::string::npos)return std::string{};
        const auto end=card.text.find(close,start+open.size());
        return card.text.substr(start+open.size(),
          end==std::string::npos?std::string::npos:end-start-open.size());
      };
      const auto out=value::trim(between("local-command-stdout"));
      const auto err=value::trim(between("local-command-stderr"));
      if(out.empty()&&err.empty())return ftxui::vbox(ftxui::Elements{});
      return gv::projectGadgetResult(err.empty()?gv::textView(out,"muted",true):
        gv::textView(out.empty()?err:out+"\n"+err,"error",true),viewStyle,columns);
    }
    if(kind==cards::UserTextKind::Interrupted)
      return gv::projectGadgetResult(gv::textView("Interrupted · What should Batbox do instead?","muted",false),
        viewStyle,columns);
    return ftxui::paragraph(value::encodeUtf8(gv::sanitize(
      card.text.empty()?card.planContent:card.text)));
  };
  out.assistant.protocol={"(no content)","No response requested.","poetry is too long",
    "Credit balance is too low",
    "Credit balance too low · Check billing with your model provider",
    "Not logged in · Please run /login","Invalid API key · Fix external API key",
    "Your ANTHROPIC_API_KEY belongs to a disabled organization · Update or unset the environment variable",
    "Your ANTHROPIC_API_KEY belongs to a disabled organization · Unset the environment variable to use your subscription instead",
    "OAuth token revoked · Please run /login","Request timed out",
    "Opus is experiencing high load, please use /model to switch to Sonnet",
    "[Request interrupted by user]","API Error",
    {"You've hit your","You've used","You're now using extra usage",
     "You're close to","You're out of extra usage"}};
  out.assistant.textImports.contextUpgradeHint=[]{return std::string{};};
  out.assistant.textImports.isKeychainLocked=[]{return false;};
  out.assistant.textImports.fallbackModelName=[this]{return inputs_.modelLabel();};
  out.assistant.textImports.startsWithApiErrorPrefix=[](const std::string& text){
    return text.starts_with("API Error")||text.starts_with("Please run /login · API Error");};
  out.assistant.projection.palette=ip::assistantPalette(*inputs_.theme);
  // Markdown keeps terminal-default body text; color its leading marker only.
  out.assistant.projection.palette.text=roles.assistantMarker;
  out.assistant.projection.dotGlyph="●";
  out.assistant.projection.renderMarkdown=[this](const std::string& text,bool dim){
    markdown_->setTerminalColumns(state_->viewport.columns);return markdown_->render(text,dim);
  };
  out.assistant.projection.renderRateLimit=[](const std::string& text){return ftxui::paragraph(value::encodeUtf8(text));};
  out.assistant.projection.renderExpandHint=[this]{return ftxui::text(inputs_.shortcuts.display("expand","global","ctrl+o"));};
  out.system.palette=ip::systemPalette(*inputs_.theme);
  out.system.referenceMark="※";out.system.errorDot="●";out.system.eventMark="·";
  out.system.renderSpecial=[](cards_system::SystemTextKind,const Json& message,
      const cards_system::SystemTextCard&,bool)->std::optional<ftxui::Element>{
    return renderComponent(message);
  };
  out.remaining.palette=ip::leafPalette(*inputs_.theme,[](bool){return true;});
  out.remaining.images.getStoredImagePath=inputs_.storedImagePath?
    inputs_.storedImagePath:[](int)->std::optional<std::string>{return {};};
  out.remaining.images.supportsHyperlinks=[this]{return inputs_.hyperlinks;};
  out.remaining.images.pathToFileURL=fileUrl;
  out.remaining.compact.getUserMessageText=userText;
  out.remaining.compact.shortcutHint=[this](const std::string& action){return inputs_.shortcuts.display(action,"global","ctrl+o");};
  out.remaining.advisor.renderModelName=[](const std::string& model){return model;};
  out.remaining.advisor.expandHint=[this]{return inputs_.shortcuts.display("expand","global","ctrl+o");};
  out.remaining.historyShortcut=[this]{return inputs_.shortcuts.display("transcript","global","ctrl+r");};
  out.remaining.attachments.readSelectedBackground=[]{return Json();};
  out.remaining.attachments.batSwarmsEnabled=[this]{return inputs_.feature("AGENT_SWARMS");};
  out.remaining.attachments.skillSearchEnabled=[this]{return inputs_.feature("SKILL_SEARCH");};
  out.remaining.attachments.isDemoEnvironment=[]{return false;};
  out.remaining.attachments.displayPath=inputs_.displayPath;
  out.remaining.attachments.isShutdownApproved=[](const std::string&){return false;};
  out.remaining.attachments.isPlanApproval=[](const std::string&,const std::string&){return false;};
  out.remaining.attachments.formatPartnerContent=[](const std::string& text)->std::optional<std::string>{return text;};
  out.remaining.attachments.lookupMission=[](const std::string&)->std::optional<Json>{return {};};
  auto& attach=out.remaining.attachmentProjection;
  attach.palette=ip::attachmentPalette(*inputs_.theme);
  attach.selectedBackground=resolve;attach.partnerColor=[](const std::string&){return ftxui::Color::Default;};
  attach.supportsHyperlinks=[this]{return inputs_.hyperlinks;};
  attach.filePathURL=fileUrl;attach.basename=basename;
  attach.expandHint=[this]{return inputs_.shortcuts.display("expand","global","ctrl+o");};
  attach.renderUserText=[](const std::string& text,bool,bool,bool){return ftxui::paragraph(value::encodeUtf8(text));};
  attach.renderImage=[](int id){return ftxui::text("[Image #"+std::to_string(id)+"]");};
  attach.renderDiagnostics=[](const Json& value,bool){return renderComponent(value);};
  attach.renderPlanApproval=[](const cards_remaining::MailboxItem& item){return ftxui::paragraph(value::encodeUtf8(item.text));};
  attach.renderPartnerText=[](const cards_remaining::MailboxItem& item,bool){return ftxui::paragraph(value::encodeUtf8(item.text));};
  attach.renderAnsi=[](const std::string& text){return ftxui::paragraph(value::encodeUtf8(text));};
  out.gadgetResult=[this,viewStyle,columns](const message_view::MessagePlan&,
      const message_view::BlockPlan&,const MessageLookups&,const gadgets::Gadgets&){
    cards_gadget_result::GadgetResultProjectionImports result;
    result.resultProtocol={
      "The user doesn't want to take this action right now. STOP what you are doing and wait for the user to tell you how to proceed.",
      "The user doesn't want to proceed with this Gadget use. The Gadget use was rejected (eg. if it was a file edit, the new_string was NOT written to the file). STOP what you are doing and wait for the user to tell you how to proceed.",
      "[Request interrupted by user for Gadget use]"};
    result.errorProtocol={"[Request interrupted by user for Gadget use]",
      "The Bat proposed a plan that was rejected by the user. The user chose to stay in plan mode rather than proceed with implementation.\n\nRejected plan:\n",
      "The user doesn't want to proceed with this Gadget use. The Gadget use was rejected (eg. if it was a file edit, the new_string was NOT written to the file). To tell you how to proceed, the user said:\n",
      "Denied by auto mode classifier"};
    result.palette=ip::gadgetErrorPalette(*inputs_.theme);
    result.columns=columns;
    result.shortcutDisplay=[this]{return inputs_.shortcuts.display("expand","global","ctrl+o");};
    result.renderRejectedPlan=[](const std::string& plan)->std::optional<ftxui::Element>{return ftxui::paragraph(value::encodeUtf8(plan));};
    result.renderGadgetError=[viewStyle,columns](const cards_gadget_result::GadgetResultCard& card,
        const Json& content,const Json& progress)->std::optional<ftxui::Element>{
      if(!card.gadget||!card.gadget->renderGadgetUseErrorMessage)return {};
      auto rendered=card.gadget->renderGadgetUseErrorMessage(content,progress);
      // Errors always read as errors, whatever shape a Gadget returns.
      if(!gv::isView(rendered))rendered=gv::errorView(gv::visibleText(rendered),false);
      return gv::projectGadgetResult(rendered,viewStyle,columns);
    };
    return result;
  };
  auto& use=out.gadgetUse;
  use.readTerminalSize=[this]{return cards_gadget_use::TerminalSize{state_->viewport.columns,state_->viewport.rows};};
  use.readTheme=[this]{return gadgetTheme(*inputs_.theme);};
  use.readSelectedBackground=[]{return Json();};
  use.readPendingWorkerGadgetID=[]()->std::optional<std::string>{return {};};
  use.readClassifierChecking=[](const std::string&){return false;};
  use.readPermissionMode=inputs_.permissionMode;
  use.readStrippedDangerousRules=[]{return false;};
  use.safeParseInput=parseInput;
  use.userFacingName=facingName;
  use.userFacingNameBackgroundColor=[](const gadgets::Gadget& gadget,const std::optional<Json>& input){
    return gadget.userFacingNameBackgroundColor?gadget.userFacingNameBackgroundColor(input.value_or(Json::object())):Json();
  };
  use.renderGadgetUse=[](const gadgets::Gadget& gadget,const Json& input,const cards_gadget_use::UseRenderContext& context){
    return gadget.renderGadgetUseMessage(input,context.theme);
  };
  use.renderTag=[](const gadgets::Gadget& gadget,const Json& input){return gadget.renderGadgetUseTag(input);};
  use.renderProgress=[](const gadgets::Gadget& gadget,const Json& progress,const cards_gadget_use::ProgressRenderContext& context){
    return gadget.renderGadgetUseProgressMessage(progress,Json{{"columns",context.terminalSize.columns},{"rows",context.terminalSize.rows}});
  };
  use.renderQueued=[](const gadgets::Gadget& gadget){return gadget.renderGadgetUseQueuedMessage();};
  use.logError=inputs_.log;
  use.stringWidth=[width=width_](const std::string& text){return static_cast<int>(width->stringWidth(Json(text)));};
  const auto spinner=[this]{return spinnerGlyph();};
  // Grouped Bats: a status glyph and title, then one row per Bat.
  const auto renderGroup=[&roles,viewStyle,spinner](const Json& value)->ftxui::Element{
    if(gv::viewKind(value)!="group")return renderComponent(value);
    const bool running=value::property(value,"running")==true;
    ftxui::Elements rows;
    rows.push_back(ftxui::hbox({
      (running?ftxui::text(spinner())|ftxui::color(roles.gadgetRunning):
        ftxui::text("✓")|ftxui::color(roles.gadgetOk))|
        ftxui::size(ftxui::WIDTH,ftxui::GREATER_THAN,2),
      ftxui::text(value::encodeUtf8(field(value,"title")))|ftxui::bold|
        ftxui::color(roles.gadgetName)}));
    ftxui::Elements entries;
    const auto& list=value::property(value,"entries");
    if(list.is_array())for(const auto& entry:list){
      if(!entry.is_string())continue;
      const auto text=entry.get<std::string>();
      const auto split=text.find(" · ");
      entries.push_back(ftxui::hbox({
        ftxui::text(value::encodeUtf8(text.substr(0,split)))|ftxui::color(roles.text),
        split==std::string::npos?ftxui::text(""):
          ftxui::text(value::encodeUtf8(text.substr(split)))|ftxui::color(roles.muted)}));
    }
    if(!entries.empty())rows.push_back(gv::gadgetResponse(ftxui::vbox(std::move(entries)),viewStyle));
    return ftxui::vbox(std::move(rows));
  };
  {
    auto& projection=out.gadgetUseProjection;
    projection.renderGadgetMessage=renderGroup;projection.resolveColor=resolve;
    projection.isBlinking=[](bool){return true;};
    projection.subtle=subtle;projection.success=success;projection.error=error;
    projection.blackCircle="●";
    projection.spinnerGlyph=spinner;
    projection.running=roles.gadgetRunning;
    projection.nameColor=roles.gadgetName;
    projection.renderInline=[target=roles.gadgetTarget](const Json& value){
      return gv::projectGadgetInline(value,target);};
    projection.renderTag=[muted=roles.muted](const Json& value){
      return gv::projectGadgetInline(value,muted);};
    projection.renderProgress=[viewStyle,columns](const Json& value){
      return gv::projectGadgetResult(gv::isView(value)?value:
        gv::textView(gv::visibleText(value),"muted",false),viewStyle,columns);};
  }
  out.groupedGadgetUse=[](const gadgets::Gadget& gadget,const std::vector<cards_gadget_use::GroupedGadgetUseEntry>& entries,bool animate,const gadgets::Gadgets&){
    Json payload=Json::array();bool running=false;
    for(const auto& entry:entries){
      payload.push_back(Json{{"param",entry.param},{"isResolved",entry.isResolved},
        {"isError",entry.isError},{"isInProgress",entry.isInProgress},
        {"progressMessages",entry.progressMessages}});
      running=running||(!entry.isResolved&&!entry.isError);
    }
    return gadget.renderGroupedGadgetUse?groupView(gadget.renderGroupedGadgetUse(payload,
      Json{{"shouldAnimate",animate}}),running):Json();
  };
  out.otherBlock=[](const message_view::MessagePlan&,const message_view::BlockPlan& block)->std::optional<ftxui::Element>{return renderComponent(block.value());};
  out.otherMessage=[](const message_view::MessagePlan& plan)->std::optional<ftxui::Element>{return renderComponent(*plan.item.message);};
  out.metadata.formatLocalTime=[](const std::string& value){return value;};
  out.metadata.stringWidth=[width=width_](const std::string& text){return static_cast<int>(width->stringWidth(Json(text)));};
  out.metadataColor=subtle;
  return out;
}
interactive::message_view::RetainedViewServices Bundle::retainedServices()const{
  using namespace interactive;
  message_view::RetainedViewServices out;
  const auto theme=inputs_.theme->currentTheme();
  const auto& roles=ip::styleRoles(theme);
  const auto subtle=roles.muted;
  const auto success=roles.gadgetOk;
  const auto error=roles.gadgetErr;
  const int columns=std::max(1,state_->viewport.columns);
  const auto viewStyle=ip::gadgetViewStyle(roles,expandKey());
  const std::function<ftxui::Element(const Json&)> renderResult=[viewStyle,columns](const Json& value){
    return gv::projectGadgetResult(value,viewStyle,columns);};
  const std::function<ftxui::Element(const Json&)> renderQuiet=[viewStyle,columns](const Json& value){
    return gv::projectGadgetResult(gv::isView(value)?value:
      gv::textView(gv::visibleText(value),"muted",false),viewStyle,columns);};
  const auto spinner=[this]{return spinnerGlyph();};
  auto& result=out.result;
  result.success.readTheme=[this]{return gadgetTheme(*inputs_.theme);};
  result.success.readBriefOnly=[]{return false;};
  result.success.getClassifierApproval=[](const std::string&)->std::optional<std::string>{return {};};
  result.success.getYoloClassifierApproval=[](const std::string&)->std::optional<std::string>{return {};};
  result.success.deleteClassifierApproval=[](const std::string&){};
  result.success.safeParseOutput=[](const gadgets::Gadget&,const Json& value)->std::optional<Json>{return value;};
  result.success.renderGadgetResult=[](const gadgets::Gadget& gadget,const Json& value,const Json& progress,
      const cards_gadget_children::SuccessRenderContext& context){
    Json theme=context.theme;
    if(context.input)theme["input"]=*context.input;
    theme["verbose"]=context.verbose;theme["transcriptMode"]=context.transcriptMode;
    return gadget.renderGadgetResultMessage(value,progress,theme);
  };
  result.success.userFacingName=[](const gadgets::Gadget& gadget){return facingName(gadget,{});};
  result.rejection.readColumns=[this]{return state_->viewport.columns;};
  result.rejection.readTheme=[this]{return gadgetTheme(*inputs_.theme);};
  result.rejection.safeParseInput=parseInput;
  result.rejection.renderRejected=[](const gadgets::Gadget& gadget,const Json& input,
      const cards_gadget_children::RejectRenderContext& context){
    return gadget.renderGadgetUseRejectedMessage(input,context.theme);
  };
  result.successProjection={columns,subtle,success,"✓",renderResult};
  result.rejectedSubtle=subtle;result.renderRejectedComponent=renderQuiet;
  auto& collapsed=out.collapsed;
  collapsed.readSelectedBackground=[]{return Json();};
  collapsed.readTheme=[this]{return gadgetTheme(*inputs_.theme);};
  collapsed.isFullscreen=[]{return true;};
  collapsed.teamMemoryEnabled=[this]{return inputs_.feature("TEAMMEM");};
  collapsed.displayPath=inputs_.displayPath;
  // Hook timings arrive in milliseconds.
  collapsed.formatDuration=seconds;
  collapsed.formatSecondsShort=seconds;
  collapsed.expandHint=[this]{return "("+expandKey()+" to expand)";};
  collapsed.stripMcpServerPrefix=[](const std::string& value){return value;};
  out.collapsedProjection={resolve,[](bool){return true;},renderComponent,
    [](const std::string& text){return ftxui::paragraph(value::encodeUtf8(gv::sanitize(text)));},
    basename,seconds,subtle,success,error,"●",spinner,roles.gadgetRunning,
    roles.text,"✓"};
  auto& verbose=out.verboseGadgets;
  verbose.readSelectedBackground=[]{return Json();};
  verbose.primitiveGadgets=inputs_.gadgets;
  verbose.safeParseInput=parseInput;
  verbose.safeParseOutput=[](const gadgets::Gadget&,const std::optional<Json>& value)->std::optional<Json>{return value;};
  verbose.userFacingName=facingName;
  verbose.renderUse=[](const gadgets::Gadget& gadget,const Json& input,const Json& theme){return gadget.renderGadgetUseMessage(input,theme);};
  verbose.renderTag=[](const gadgets::Gadget& gadget,const Json& input){return gadget.renderGadgetUseTag?gadget.renderGadgetUseTag(input):Json();};
  verbose.renderResult=[](const gadgets::Gadget& gadget,const Json& output,
      const gadgets::Gadgets&,const Json& theme){
    // The verbose list is the expanded view: Gadgets show full output.
    Json expanded=theme.is_object()?theme:Json::object();
    expanded["verbose"]=true;
    return gadget.renderGadgetResultMessage?
      gadget.renderGadgetResultMessage(output,Json::array(),expanded):Json();};
  out.verboseProjection.renderComponent=renderComponent;
  out.verboseProjection.resolveColor=resolve;
  out.verboseProjection.isBlinking=[](bool){return true;};
  out.verboseProjection.subtle=subtle;out.verboseProjection.success=success;
  out.verboseProjection.error=error;out.verboseProjection.blackCircle="●";
  out.verboseProjection.spinnerGlyph=spinner;
  out.verboseProjection.running=roles.gadgetRunning;
  out.verboseProjection.renderInline=[target=roles.gadgetTarget](const Json& value){
    return gv::projectGadgetInline(value,target);};
  out.verboseProjection.renderResult=renderResult;
  return out;
}
} // namespace batbox::application_presentation
