#include "interactive/conversation_shell/Shell.hpp"
#include "application_editor/IndigenousProcess.hpp"
#include "application_editor/TranscriptFile.hpp"
#include "npm_wrap_ansi/Internal.hpp"
#include "runtime/JsonValue.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace batbox::interactive::conversation_shell {
struct TranscriptExportRun {
  std::shared_ptr<PreparedMessageList> prepared;
  std::shared_ptr<application_editor::TranscriptFile> file;
  std::size_t next=0;
  int columns=80;
};
namespace {
std::string trimLineEnds(std::string text){
  std::string output;output.reserve(text.size());
  std::size_t start=0;
  while(start<text.size()){
    const auto end=text.find('\n',start);
    auto last=end==std::string::npos?text.size():end;
    while(last>start&&(text[last-1]==' '||text[last-1]=='\t'))--last;
    output.append(text,start,last-start);
    if(end==std::string::npos)break;
    output+='\n';start=end+1;
  }
  return output;
}
std::string safeTerminalText(std::string_view value){
  std::string result;result.reserve(value.size());
  for(const unsigned char ch:value)
    if((ch>=0x20&&ch!=0x7f)||ch=='\n'||ch=='\t')
      result+=static_cast<char>(ch);
  return result;
}
}
void Shell::openNativeTranscriptCopy(std::optional<std::string> selection){
  if(!transcript_||!started_||driver_->mode()!=terminal_ftxui::Mode::Fullscreen)
    return;
  try{
    driver_->withRestoredIO([this,&selection]{
      std::cout<<"\nBATBOX TRANSCRIPT  /  NATIVE COPY\n\n";
      if(selection)
        std::cout<<safeTerminalText(*selection)<<"\n";
      else for(const auto& row:presentation().transcriptRows)
        std::cout<<safeTerminalText(row.text)<<"\n\n";
      std::cout<<"\nSelect text and use your terminal's copy shortcut. "
        "Press Return to return to Batbox.\n"<<std::flush;
      std::string ignored;
      std::getline(std::cin,ignored);
      if(!std::cin)std::cin.clear();
    });
  }catch(const std::exception& issue){
    transcriptStatus(std::string("native copy failed: ")+issue.what());
  }
}
void Shell::transcriptStatus(const std::string& value)const{
  if(imports_.editorCallbacks.addNotification)
    imports_.editorCallbacks.addNotification({"transcript-editor-status",
      value,"immediate",4000});
}
std::string Shell::renderTranscriptChunk(const PreparedMessageList& full,
    std::size_t begin,std::size_t end,int columns){
  auto prepared=std::make_shared<PreparedMessageList>(full);
  prepared->renderable=std::make_shared<RenderItems>(
    full.collapsed->begin()+static_cast<std::ptrdiff_t>(begin),
    full.collapsed->begin()+static_cast<std::ptrdiff_t>(end));
  auto options=imports_.surfaceOptions(presentation());
  // exportRenderer renders Messages in poetry screen mode even when
  // invoked by transcript `v`.
  options.screen="prompt";
  options.conversationId="transcript-export";
  options.columns=columns;
  options.showAllInTranscript=true;
  options.disableRenderCap=true;
  options.virtualScroll=false;
  options.hideLogo=true;
  options.isLoading=false;
  options.streamingText.reset();options.streamingThinking.reset();
  options.unseenDivider.reset();options.cursor.reset();
  options.inProgressGadgetUseIDs.clear();options.streamingGadgetUseIDs.clear();
  options.renderRange=std::pair{begin,end};
  message_list_view::SurfaceState surface;
  auto plan=message_list_view::prepareSurface(prepared,options,surface,
    imports_.nowMs(),imports_.messageImports(),imports_.decisionImports());
  auto retained=message_view::RetainedViews::create(imports_.owner,
    imports_.nowMs,[]{});
  retained->beginFrame(options.conversationId);
  message_list_view::ProjectionImports renderer;
  renderer.messages=retained->bind(imports_.projectionServices(),
    imports_.retainedViews);
  renderer.renderStreamingMarkdown=imports_.renderStreamingMarkdown;
  renderer.renderLogo=imports_.chrome.renderLogo;
  renderer.renderDivider=imports_.chrome.renderDivider;
  auto element=message_list_view::projectSurface(plan,renderer)|
    ftxui::size(ftxui::WIDTH,ftxui::EQUAL,columns);
  const auto dimensions=ftxui::Dimension::Fit(element,true);
  if(dimensions.dimy>8192)
    throw std::runtime_error("Transcript chunk exceeds Indigenous render height");
  if(dimensions.dimy<=0)return {};
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
    ftxui::Dimension::Fixed(dimensions.dimy));
  ftxui::Render(screen,element);
  auto plain=value::fromUtf16(npm_wrap_ansi::stripAnsi(
    value::utf16CodeUnits(screen.ToString())));
  retained->unmountAll();
  return trimLineEnds(std::move(plain));
}
void Shell::startTranscriptExport(){
  if(!imports_.owner.isOwnerThread())
    throw std::logic_error("Transcript export requires owner loop");
  if(transcriptExport_||!transcript_||!started_)return;
  try{
    auto settings=imports_.listSettings(presentation());
    settings.transcriptMode=false;
    settings.showAllInTranscript=true;
    settings.disableRenderCap=true;
    settings.virtualScroll=false;
    settings.fullscreen=true;
    settings.renderRange.reset();
    settings.streamingGadgetUses.clear();settings.inProgressGadgetUseIDs.clear();
    std::optional<SliceAnchor> anchor;
    auto run=std::make_shared<TranscriptExportRun>();
    run->prepared=std::make_shared<PreparedMessageList>(
      prepareMessageList(presentation().messages,settings,anchor));
    run->file=application_editor::TranscriptFile::create();
    run->columns=std::max(80,presentation().viewport.columns-6);
    transcriptExport_=run;
    transcriptStatus("rendering "+
      std::to_string(presentation().messages.size())+" messages…");
    advanceTranscriptExport();
  }catch(const std::exception& issue){
    transcriptExport_.reset();
    transcriptStatus(std::string("render failed: ")+issue.what());
  }
}
void Shell::advanceTranscriptExport(){
  auto run=transcriptExport_;
  if(!run||!started_||!transcript_)return;
  try{
    const auto total=run->prepared->collapsed->size();
    if(run->next<total){
      const auto end=std::min(total,run->next+40);
      run->file->append(renderTranscriptChunk(*run->prepared,run->next,
        end,run->columns));
      run->next=end;
      auto weak=weak_from_this();
      imports_.owner.queueNextTurn([weak]{
        if(auto self=weak.lock())self->advanceTranscriptExport();});
      return;
    }
    run->file->finish();
    const auto path=run->file->path().string();
    const auto editor=application_editor::indigenous::externalEditor();
    bool opened=false;
    if(editor){
      if(editor->gui)
        opened=application_editor::indigenous::launchDetached(*editor,path);
      else{
        application_editor::indigenous::ProcessResult result;
        driver_->withRestoredIO([&]{
          result=application_editor::indigenous::runInherited(*editor,path);});
        opened=!result.error;
      }
    }
    transcriptStatus(opened?"opening "+path:
      "wrote "+path+" · no $VISUAL/$EDITOR set");
  }catch(const std::exception& issue){
    transcriptStatus(std::string("render failed: ")+issue.what());
  }
  transcriptExport_.reset();
}
} // namespace batbox::interactive::conversation_shell
