#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/conversation_shell/PoetryViewport.hpp"
#include "interactive/presentation_providers/MissionFooter.hpp"
#include "terminal_ftxui/ElementSearch.hpp"
#include "terminal_ftxui/TextFit.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::interactive::conversation_shell {
namespace {
int heightOf(const ftxui::Element& element){
  if(!element)return 0;
  element->ComputeRequirement();return std::max(0,element->requirement().min_y);
}
ftxui::Element fixed(ftxui::Element element,int rows){
  return element|ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,std::max(0,rows));
}
class BottomOverlay final:public ftxui::Node {
public:
  BottomOverlay(ftxui::Element base,ftxui::Element overlay,int height):
    Node({std::move(base),std::move(overlay)}),height_(height){}
  void ComputeRequirement()override{
    children_[0]->ComputeRequirement();children_[1]->ComputeRequirement();
    requirement_=children_[0]->requirement();
  }
  void SetBox(ftxui::Box box)override{
    box_=box;children_[0]->SetBox(box);
    auto lower=box;lower.y_min=std::max(box.y_min,box.y_max-height_+1);
    children_[1]->SetBox(lower);
  }
  void Render(ftxui::Screen& screen)override{
    children_[0]->Render(screen);
    // Paragraph whitespace is transparent in the DOM. Modal rows must cover
    // the underlying live transcript, including after an editor handoff.
    ftxui::Cell blank;blank.character=" ";
    const int first=std::max({0,box_.y_min,box_.y_max-height_+1});
    for(int y=first;y<=std::min(box_.y_max,screen.dimy()-1);++y)
      for(int x=std::max(0,box_.x_min);x<=std::min(box_.x_max,screen.dimx()-1);++x)
        screen.CellAt(x,y)=blank;
    children_[1]->Render(screen);
  }
private:
  int height_;
};
std::string repeatRule(int columns){
  std::string value;for(int i=0;i<std::max(1,columns);++i)value+="▔";
  return value;
}
std::string oneLine(std::string value,std::size_t width){
  const auto breakAt=value.find_first_of("\r\n");
  if(breakAt!=std::string::npos)value.resize(breakAt);
  // Width is in terminal columns; multibyte glyphs are never split.
  return terminal_ftxui::truncateColumns(value,static_cast<int>(width));
}
}
const ViewState Shell::viewState()const{
  const auto& view=presentation().viewport;
  const bool pill=!viewport_->atBottom();
  auto result=ViewState{presentation(),sticky_,cursor_,
    searchOpen_?searchEditor_->view(searchInput_):
      editor_->view(presentation().input),navigation_->searchQuery(),
    searchCount_,searchCurrent_,newMessageCount_,view.columns,view.rows,
    transcript_,searchOpen_,pill};
  if(imports_.missionSnapshot)result.backgroundMissions=
    presentation_providers::backgroundFooterMissions(imports_.missionSnapshot());
  if(batConversation_){
    result.batId=batConversation_->id;
    result.batLabel=batConversation_->label;
    result.batStatus=batConversation_->status;
  }
  result.remembering=remembering_;
  result.activityFrame=activityFrame_;
  result.welcomeFrame=welcomeFrame_;
  if(workingSince_&&imports_.nowMs)
    result.workingMs=std::max(0.0,imports_.nowMs()-*workingSince_);
  result.streamMode=presentation().streamMode.value_or("");
  result.historySearch=!batConversation_&&historyNavigator_&&historyNavigator_->searching();
  return result;
}
int Shell::poetryColumns(int columns)const{
  return imports_.chrome.poetryDecor&&columns>=24?columns-poetryFrameInset:columns;
}
ftxui::Element Shell::renderInline(){
  const auto& view=presentation().viewport;
  return terminal_ftxui::renderPresentationWithScrollback(presentation(),
    ftxui::Dimensions{view.columns,view.rows},scrollbackCursor_,true);
}
ftxui::Element Shell::renderFullscreen(const message_list_view::SurfacePlan& plan){
  retained_->beginFrame(plan.options.conversationId);
  auto projected=imports_.projectionServices();
  currentGadgets_=projected.gadgets;
  message_list_view::ProjectionImports renderer;
  renderer.messages=retained_->bind(std::move(projected),
    imports_.liveRetainedViews?imports_.liveRetainedViews():imports_.retainedViews);
  renderer.renderStreamingMarkdown=imports_.renderStreamingMarkdown;
  renderer.renderLogo=imports_.chrome.renderLogo;
  renderer.renderDivider=imports_.chrome.renderDivider;
  renderer.renderVirtualList=[this](const message_list_view::SurfacePlan& source,
      const message_list_view::RowRenderer& rows){
    return viewport_->projectList(source,rows);
  };
  auto view=viewState();
  auto header=imports_.chrome.renderHeader(view);
  // With a status row the chrome sits under the input, where the eyes are.
  const bool statusBelow=static_cast<bool>(imports_.chrome.renderStatus);
  auto footer=statusBelow?imports_.chrome.renderStatus(view):
    imports_.chrome.renderFooter(view);
  auto modal=imports_.chrome.renderModal(view);
  auto pill=imports_.chrome.renderPill(view);
  auto missions=imports_.chrome.renderMissions?imports_.chrome.renderMissions(view):
    ftxui::vbox(ftxui::Elements{});
  const bool showSticky=sticky_.kind==transcript_navigation::StickyPoetry::Kind::Poetry&&
    !presentation().modal;
  auto sticky=showSticky?fixed(terminal_ftxui::fitText(presentation().input.poetry+
    oneLine(sticky_.text,static_cast<std::size_t>(std::max(8,view.columns))))|ftxui::dim,1):
    ftxui::vbox(ftxui::Elements{});
  const auto& input=searchOpen_?searchInput_:presentation().input;
  const bool showPoetry=searchOpen_||!presentation().modal||
    !presentation().modal->hidePoetry;
  const auto decor=imports_.chrome.poetryDecor?
    std::optional<PoetryDecor>(imports_.chrome.poetryDecor(view)):std::nullopt;
  const bool framed=decor&&poetryColumns(view.columns)!=view.columns;
  const int poetryLimit=std::max(1,view.rows/2-heightOf(footer)-(framed?2:0));
  auto poetry=showPoetry?poetryViewport(input,poetryColumns(view.columns),poetryLimit,
    imports_.editorMetrics,searchOpen_?terminalFocused_:input.focused,
    decor?&*decor:nullptr):ftxui::vbox(ftxui::Elements{});
  if(showPoetry&&framed){
    poetry=ftxui::hbox({ftxui::text(" "),std::move(poetry)|ftxui::xflex,ftxui::text(" ")})|
      ftxui::borderStyled(ftxui::ROUNDED,decor->border);
  }
  const bool showSuggestions=!transcript_&&!searchOpen_&&
    !presentation().modal&&!suggestions_.empty()&&input.focused;
  ftxui::Elements suggestionLines;
  ftxui::Element suggestions;
  if(showSuggestions&&imports_.chrome.renderSuggestions){
    suggestions=imports_.chrome.renderSuggestions(suggestions_,selectedSuggestion_,view);
  }else{
  if(showSuggestions){
    const auto shown=std::min<std::size_t>(6,suggestions_.size());
    const auto first=selectedSuggestion_>=shown?
      selectedSuggestion_-shown+1:0;
    for(std::size_t offset=0;offset<shown;++offset){
      const auto index=first+offset;
      if(index>=suggestions_.size())break;
      const auto& suggestion=suggestions_[index];
      const bool selected=index==selectedSuggestion_;
      auto line=ftxui::text(oneLine(
        std::string(selected?"❯ ":"  ")+suggestion.display+
          (suggestion.description.empty()?"":"  "+suggestion.description),
        static_cast<std::size_t>(std::max(8,view.columns-2))));
      if(selected)line=line|ftxui::bold|ftxui::inverted;
      else line=line|ftxui::dim;
      suggestionLines.push_back(std::move(line));
    }
  }
  suggestions=ftxui::vbox(std::move(suggestionLines));
  }
  const bool showHistorySearch=view.historySearch;
  auto historySearch=showHistorySearch?ftxui::text(oneLine(
      std::string("reverse-i-search: ")+historyNavigator_->searchQuery()+
        (historyNavigator_->searchFailed()?"  (no further match)":""),
      static_cast<std::size_t>(std::max(8,view.columns-2))))|ftxui::dim:
    ftxui::vbox(ftxui::Elements{});
  int headerRows=heightOf(header),footerRows=heightOf(footer);
  int poetryRows=showPoetry?std::max(framed?3:1,heightOf(poetry)):0;
  int suggestionRows=showSuggestions?heightOf(suggestions):0;
  int missionRows=heightOf(missions);
  const int historySearchRows=showHistorySearch?1:0;
  const int pillRows=view.pillVisible&&!presentation().modal?1:0;
  const auto rows=std::max(1,view.rows);
  footerRows=std::min(footerRows,rows/2);
  missionRows=std::min(missionRows,std::max(0,rows/4));
  poetryRows=std::min(poetryRows,std::max(framed?3:0,rows/2-footerRows));
  suggestionRows=std::min(suggestionRows,
    std::max(0,rows-footerRows-poetryRows-historySearchRows-pillRows-missionRows-2));
  headerRows=std::min(headerRows,
    std::max(0,rows-footerRows-poetryRows-suggestionRows-
      historySearchRows-pillRows-missionRows-1));
  const int stickyRows=showSticky?1:0;
  const int bodyRows=std::max(1,rows-headerRows-footerRows-poetryRows-
    suggestionRows-historySearchRows-stickyRows-pillRows-missionRows);
  stickyScreenRow_=showSticky?std::optional<int>(headerRows):std::nullopt;
  pillScreenRow_=pillRows?std::optional<int>(rows-poetryRows-1-
    (statusBelow?footerRows:0)):std::nullopt;
  missionScreenRow_=missionRows?std::optional<int>(headerRows+stickyRows+bodyRows+
    suggestionRows+historySearchRows):std::nullopt;
  footerMissionIds_.clear();
  for(std::size_t i=0;i<view.backgroundMissions.size()&&
      i<static_cast<std::size_t>(std::max(0,missionRows-1));++i)
    footerMissionIds_.push_back(view.backgroundMissions[i].at("id").get<std::string>());
  view.bodyRows=bodyRows;view.bodyScreenRow=headerRows+stickyRows;
  welcomeVisible_=plan.showLogo&&imports_.chrome.renderWelcome&&
    !transcript_&&!batConversation_;
  welcomeRendered_=true;
  if(welcomeVisible_)renderer.renderLogo=[this,view]{
    return imports_.chrome.renderWelcome(view);
  };
  // Keep the welcome before the measured list so its height contributes to
  // the list origin and it scrolls with messages, including the first turn.
  auto surface=message_list_view::projectSurface(plan,renderer);
  auto scroll=viewport_->wrap(std::move(surface),bodyRows);
  if(transcript_){
    auto weak=weak_from_this();
    scroll=terminal_ftxui::withSearchOverlay(std::move(scroll),
      [weak]{if(auto self=weak.lock())return self->navigation_->searchQuery();
        return std::string{};},
      [weak]()->std::optional<terminal_ftxui::PositionedMatches>{
        auto self=weak.lock();if(!self||!self->positions_)return {};
        const auto& current=*self->positions_;
        return terminal_ftxui::PositionedMatches{current.positions,
          current.rowOffset,static_cast<double>(current.currentIdx)};
      });
  }
  ftxui::Elements stack;
  if(headerRows)stack.push_back(fixed(std::move(header),headerRows));
  if(stickyRows)stack.push_back(std::move(sticky));
  stack.push_back(std::move(scroll));
  if(suggestionRows)stack.push_back(fixed(std::move(suggestions),
    suggestionRows));
  if(historySearchRows)stack.push_back(fixed(std::move(historySearch),1));
  if(missionRows)stack.push_back(fixed(std::move(missions),missionRows));
  if(footerRows&&!statusBelow)stack.push_back(fixed(std::move(footer),footerRows));
  if(pillRows)stack.push_back(fixed(std::move(pill),pillRows));
  if(poetryRows)stack.push_back(fixed(std::move(poetry),poetryRows));
  if(footerRows&&statusBelow)stack.push_back(fixed(std::move(footer),footerRows));
  auto base=ftxui::vbox(std::move(stack));
  if(presentation().modal){
    const bool bounded=presentation().modal->bodyPreviewLines.has_value();
    const int ruleRows=bounded&&rows<=5?0:1;
    const int maximum=std::max(1,rows-(bounded?ruleRows:4));
    const int modalRows=std::min(maximum,heightOf(modal));
    ftxui::Elements overlay;
    if(ruleRows)overlay.push_back(ftxui::text(repeatRule(view.columns)));
    overlay.push_back(std::move(modal));
    base=std::make_shared<BottomOverlay>(std::move(base),
      fixed(ftxui::vbox(std::move(overlay))|
        ftxui::bgcolor(ftxui::Color::Default),modalRows+ruleRows),modalRows+ruleRows);
  }
  return fixed(std::move(base),rows);
}
ftxui::Element Shell::render(){
  refreshBatConversation();
  syncActivityPulse();
  if(copyView_)return copyView_->render();
  if(driver_->mode()==terminal_ftxui::Mode::Inline)return renderInline();
  auto settings=imports_.listSettings(presentation());
  settings.fullscreen=true;settings.transcriptMode=transcript_;
  settings.virtualScroll=true;
  settings.streamingGadgetUses=presentation().streamingGadgetUses;
  prepared_=std::make_shared<PreparedMessageList>(prepareMessageList(
    presentation().messages,settings,sliceAnchor_));
  auto options=imports_.surfaceOptions(presentation());
  options.screen=transcript_?"transcript":"prompt";
  options.conversationId=presentation().conversationId;
  options.columns=presentation().viewport.columns;
  options.virtualScroll=true;
  options.isLoading=presentation().loading;
  options.streamingText=presentation().showStreamingText?
    presentation().streamingText:std::nullopt;
  const auto& thinking=presentation().streamingThinking;
  if(thinking.has_value()!=previousThinking_.has_value()||
      (thinking&&previousThinking_&&
       (thinking->thinking!=previousThinking_->thinking||
        thinking->isStreaming!=previousThinking_->isStreaming||
        thinking->streamingEndedAt!=previousThinking_->streamingEndedAt))){
    previousThinking_=thinking;
    thinkingRef_=thinking?std::make_shared<const StreamingThinking>(*thinking):nullptr;
  }
  options.streamingThinking=thinkingRef_;
  newMessageCount_=options.unseenDivider?options.unseenDivider->count:0;
  lastHasGadgetsInProgress_=!options.inProgressGadgetUseIDs.empty();
  auto plan=message_list_view::prepareSurface(prepared_,std::move(options),
    surfaceState_,imports_.nowMs(),imports_.messageImports(),
    imports_.decisionImports());
  dividerIndex_.reset();
  if(plan.options.unseenDivider){
    const auto prefix=value::slice(plan.options.unseenDivider->firstUnseenUuid,0,24);
    for(std::size_t i=0;i<plan.rows.size();++i){
      if(value::slice(plan.rows[i].key,0,24)==prefix){dividerIndex_=i;break;}
    }
  }
  navigation_->setItems(prepared_->renderable);
  actions_->setItems(prepared_->renderable);
  return renderFullscreen(plan);
}
ftxui::Element Shell::renderSnapshot(){return render();}
void Shell::commitFrame(){committed();}
void Shell::committed(){
  if(driver_->mode()==terminal_ftxui::Mode::Inline)return;
  syncWelcomeAnimation();
  viewport_->committed();
  retained_->committed();
  surfaceState_.committed(imports_.terminalProgressEnabled(),
    lastHasGadgetsInProgress_,imports_.setTerminalProgress);
  navigation_->afterPaint();navigation_->observeSticky();
}
void Shell::commitInlineScrollback(){
  if(!driver_||driver_->mode()!=terminal_ftxui::Mode::Inline||
     !driver_->interactive())return;
  const auto& view=presentation().viewport;
  auto rows=terminal_ftxui::advanceInlineScrollback(presentation(),
    ftxui::Dimensions{view.columns,view.rows},scrollbackCursor_,true);
  driver_->commitScrollback(rows);
}
void Shell::onResize(ftxui::Dimensions size){
  const auto prior=presentation().viewport.columns;
  updateViewport(presentation(),size.dimx,size.dimy);
  if(copyView_){
    if(size.dimx>0&&size.dimy>=3)copyView_->resize(presentation(),size.dimx,size.dimy);
    else{copyScrollTimer_.cancel();copyScrollTimer_={};copyView_.reset();}
  }
  if(batConversation_)updateViewport(imports_.presentation,size.dimx,size.dimy);
  auto options=imports_.editorOptions;
  options.columns=poetryEditorColumns(presentation().input,poetryColumns(size.dimx),
    imports_.editorMetrics);
  options.fullscreen=driver_->mode()==terminal_ftxui::Mode::Fullscreen;
  editor_->setOptions(std::move(options));
  auto searchOptions=imports_.editorOptions;
  searchOptions.columns=poetryEditorColumns(searchInput_,poetryColumns(size.dimx),
    imports_.editorMetrics);
  searchOptions.fullscreen=driver_->mode()==terminal_ftxui::Mode::Fullscreen;
  searchOptions.multiline=false;searchOptions.disableEscapeDoublePress=true;
  searchEditor_->setOptions(std::move(searchOptions));
  if(prior!=size.dimx&&(searchOpen_||!searchQuery_.empty())){
    searchOpen_=false;searchQuery_.clear();searchDraft_.clear();
    searchInput_.text.clear();searchInput_.caretUtf16=0;
    searchCount_=searchCurrent_=0;navigation_->disarmSearch();
    navigation_->setSearchQuery("");
  }
  commitInlineScrollback();requestRedraw();
}
} // namespace batbox::interactive::conversation_shell
