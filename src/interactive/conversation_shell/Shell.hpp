#pragma once
#include "interactive/InteractionController.hpp"
#include "interactive/message_actions/MessageActions.hpp"
#include "interactive/message_list_view/Projection.hpp"
#include "interactive/message_list_view/Search.hpp"
#include "interactive/message_view/RetainedViews.hpp"
#include "interactive/repl_runtime/Runtime.hpp"
#include "interactive/text_input/TextEditor.hpp"
#include "interactive/transcript_navigation/Navigation.hpp"
#include "interactive/viewport/Controller.hpp"
#include "terminal_ftxui/TranscriptCopy.hpp"
#include "application_history/History.hpp"
#include "application_completion/Completion.hpp"
#include <ftxui/dom/node.hpp>
#include <optional>
#include <deque>
#include <string_view>

namespace batbox::interactive::conversation_shell {
struct TranscriptExportRun;
inline constexpr unsigned welcomeAnimationFrames=12;
struct ViewState {
  const PresentationState& presentation;
  const transcript_navigation::StickyPoetry& sticky;
  const std::optional<message_actions::Cursor>& cursor;
  text_input::EditorView editor;
  std::string_view searchQuery;
  std::size_t searchCount=0,searchCurrent=0,newMessageCount=0;
  int columns=80,rows=24;
  bool transcript=false,searchOpen=false,pillVisible=false;
  Json backgroundMissions=Json::array();
  std::optional<std::string> batId;
  std::string batLabel,batStatus;
  bool remembering=false;
  unsigned activityFrame=0;
  unsigned welcomeFrame=welcomeAnimationFrames;
  int bodyRows=0,bodyScreenRow=0;
};
struct ChromeProviders {
  std::function<ftxui::Element(const ViewState&)> renderHeader,
    renderFooter,renderModal,renderPill;
  std::function<ftxui::Element(const ViewState&)> renderMissions;
  std::function<ftxui::Element()> renderLogo;
  std::function<ftxui::Element(const ViewState&)> renderWelcome;
  std::function<ftxui::Element(const message_list_view::DividerRequest&)>
    renderDivider;
};
struct Imports {
  event_loop::Loop& owner;
  PresentationState& presentation;
  terminal_ftxui::Options driverOptions;
  Imports(event_loop::Loop& loop,PresentationState& state,
    terminal_ftxui::Options options):
    owner(loop),presentation(state),driverOptions(options){}
  InteractionImports input;
  std::shared_ptr<submit::SubmitServices> submitServices;
  std::function<submit::PoetrySubmitRequest(const std::string&,
    const InputState&)> makeSubmitRequest;
  std::function<void()> onRepin;
  std::function<bool()> backgroundForegroundMissions;
  std::function<Json()> missionSnapshot;
  std::function<void(const std::string&)> openBackgroundMission;
  std::function<bool(const std::string&,const std::string&)> openControlCommand;
  // Bat mission observation is independent of the leader's runtime/message owner.
  std::function<event_loop::Deferred<Json>(const std::string&)> observeBatMission;
  std::function<std::optional<Json>(const std::string&)> batMissionSnapshot;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&)> submitBatMessage;
  std::function<void(const std::string&)> releaseBatMission,stopBatMission;
  std::function<std::size_t()> getSubmitCount;
  std::function<bool()> interruptTurn;
  std::function<void(const std::string&)> copyMessageText;
  std::function<event_loop::Deferred<event_loop::Unit>(MessageRef)> editMessage;
  std::function<event_loop::Deferred<Json>()> getImageFromClipboard;
  std::function<event_loop::Deferred<Json>(const std::string&)> readPastedImagePaths;
  std::function<void(const Json&)> onImagePasted;
  std::shared_ptr<application_history::Bundle> history;
  std::shared_ptr<application_completion::Provider> completion;
  std::function<MessageListSettings(const PresentationState&)> listSettings;
  std::function<message_list_view::SurfaceOptions(const PresentationState&)>
    surfaceOptions;
  std::function<message_view::MessageImports()> messageImports;
  std::function<MessageDecisionImports()> decisionImports;
  std::function<message_view::ProjectionServices()> projectionServices;
  std::function<ftxui::Element(const std::string&,bool)> renderStreamingMarkdown;
  message_view::RetainedViewServices retainedViews;
  ChromeProviders chrome;
  text_input::EditorOptions editorOptions;
  text_input::MetricsServices editorMetrics;
  text_input::EditorCallbacks editorCallbacks;
  std::function<double()> nowMs;
  std::function<void(const std::string&)> logForDebugging;
  std::function<bool()> terminalProgressEnabled;
  std::function<void(std::optional<message_list_view::ProgressState>)>
    setTerminalProgress;
  std::function<void(std::exception_ptr)> onDetachedError;
};

class Shell final:public std::enable_shared_from_this<Shell> {
public:
  static std::shared_ptr<Shell> create(Imports imports);
  ~Shell();
  void bindRuntime(std::shared_ptr<repl_runtime::Runtime> runtime);
  // Share the command router's UI state so terminal keys operate on the
  // command that the slash dispatcher opened.
  void attachCommandUi(std::shared_ptr<CommandUi> commandUi);
  // Roost resume replaces slash-command providers after the Shell starts.
  // Keep the current command/modal owner until its completion callback settles.
  void replaceCommandUi(std::shared_ptr<CommandUi> commandUi);
  void setCommandFocus(bool focused);
  void start();
  void stop();
  void requestRedraw();
  // Transient UI state around project recovery; no conversation message.
  void setRemembering(bool active);
  // Also usable by a headless FTXUI render check; Driver calls both in order.
  ftxui::Element renderSnapshot();
  void commitFrame();
  void onRawInput(std::string_view bytes);
  void applyStream(StreamInput input);
  // Call after a direct message-list replacement by the application owner.
  void messagesChanged();
  void setTranscript(bool active);
  void openSearch();
  void setSearchQuery(std::string query);
  void closeSearch(bool commit);
  void dispatchMessageAction(const std::string& action);
  void scroll(double rows);
  void jumpToBottom();
  void resetHistory();
  void openBatConversation(const std::string& id);
  void returnToLeader();
  std::optional<std::string> activeBatId()const;
  PresentationState& presentation();
  const PresentationState& presentation()const;
  terminal_ftxui::Driver& driver(){return *driver_;}
  CommandUi& commands(){return *commands_;}
  viewport::Controller& viewport(){return *viewport_;}
  transcript_navigation::Navigation& navigation(){return *navigation_;}
  message_actions::MessageActionsController& actions(){return *actions_;}
  const std::optional<transcript_navigation::PositionOverlay>& positions()const{
    return positions_;}
  const ViewState viewState()const;
private:
  explicit Shell(Imports imports);
  void initialize();
  ftxui::Element render();
  ftxui::Element renderFullscreen(const message_list_view::SurfacePlan& plan);
  ftxui::Element renderInline();
  void committed();
  void onResize(ftxui::Dimensions size);
  void onParsedInput(const keyboard_input::ParsedInput& input);
  void onKey(const keyboard_input::KeyRef& key);
  void onClipboardImage(const Json& image);
  void refreshCompletions();
  void acceptCompletion(bool execute);
  void onModalKey(const keyboard_input::KeyRef& key);
  void onSearchKey(const keyboard_input::KeyRef& key);
  bool onCopyMouse(const ftxui::Mouse& mouse);
  void scheduleCopyScroll();
  void submitPoetry(const std::string& text);
  void onPastedText(const std::string& text);
  void startTranscriptExport();
  void advanceTranscriptExport();
  void openNativeTranscriptCopy(std::optional<std::string> selection={});
  void transcriptStatus(const std::string& value)const;
  std::string renderTranscriptChunk(const PreparedMessageList& prepared,
    std::size_t begin,std::size_t end,int columns);
  void syncFocus();
  void syncActivityPulse();
  void syncWelcomeAnimation();
  void commitInlineScrollback();
  void observeLastMessage();
  void reportDetached(std::exception_ptr error)const;
  void finishCommandUiReplacement();
  void refreshBatConversation();
  void updateBatConversation(const Json& mission);
  bool submitBatPoetry(const std::string& text);
  void resetConversationNavigation();
  struct BatConversation {
    std::string id,label,status;
    PresentationState presentation;
    Json messages=Json::array();
    bool submitting=false;
    // Display-only delivery state for follow-ups sent while the Bat runs.
    std::string statusLine;
    bool queued=false,delivered=false;
    std::size_t deliveredAfter=0;
  };
  Imports imports_;
  std::optional<BatConversation> batConversation_;
  std::unordered_map<std::string,InputState> batDrafts_;
  std::size_t batOpenGeneration_=0;
  std::optional<double> leaderScrollTop_;
  bool leaderAtBottom_=true;
  std::shared_ptr<repl_runtime::Runtime> runtime_;
  std::shared_ptr<terminal_ftxui::Driver> driver_;
  std::shared_ptr<CommandUi> commands_;
  std::shared_ptr<CommandUi> pendingCommandUi_;
  event_loop::Loop::TimerHandle commandUiSwapTimer_;
  event_loop::Loop::TimerHandle activityPulseTimer_;
  event_loop::Loop::TimerHandle welcomeTimer_;
  unsigned activityFrame_=0;
  unsigned welcomeFrame_=welcomeAnimationFrames;
  bool welcomeVisible_=false,welcomeRendered_=false,welcomeStarted_=false;
  bool remembering_=false;
  std::shared_ptr<viewport::Controller> viewport_;
  std::shared_ptr<transcript_navigation::Navigation> navigation_;
  std::shared_ptr<message_view::RetainedViews> retained_;
  std::unique_ptr<message_actions::MessageActionsController> actions_;
  std::unique_ptr<text_input::TextEditor> editor_;
  std::shared_ptr<application_history::Navigator> historyNavigator_;
  std::vector<application_completion::Suggestion> suggestions_;
  std::deque<keyboard_input::KeyRef> bufferedPasteKeys_;
  bool pastePending_=false,pendingImageSpace_=false;
  std::shared_ptr<TranscriptExportRun> transcriptExport_;
  std::size_t selectedSuggestion_=0,completionGeneration_=0;
  std::unique_ptr<text_input::TextEditor> searchEditor_;
  InputState searchInput_;
  text_input::KillRing killRing_;
  message_list_view::SurfaceState surfaceState_;
  std::optional<StreamingThinking> previousThinking_;
  std::shared_ptr<const StreamingThinking> thinkingRef_;
  std::optional<Json> streamingThinkingIndex_;
  message_list_view::SearchTextStash searchText_;
  std::optional<SliceAnchor> sliceAnchor_;
  std::shared_ptr<PreparedMessageList> prepared_;
  MessageRef lastObservedMessage_;
  keyboard_input::KeyParseState parser_;
  event_loop::Loop::TimerHandle incompleteEscapeTimer_;
  event_loop::Loop::TimerHandle copyScrollTimer_;
  terminal_ftxui::PlainCursor plainCursor_;
  terminal_ftxui::ScrollbackCursor scrollbackCursor_;
  std::unique_ptr<terminal_ftxui::TranscriptCopy> copyView_;
  terminal_ftxui::Mode baseMode_=terminal_ftxui::Mode::Inline;
  gadgets::Gadgets currentGadgets_;
  std::optional<std::size_t> dividerIndex_;
  std::optional<int> stickyScreenRow_,pillScreenRow_;
  std::optional<int> missionScreenRow_;
  std::vector<std::string> footerMissionIds_;
  std::optional<transcript_navigation::PositionOverlay> positions_;
  transcript_navigation::StickyPoetry sticky_;
  std::optional<message_actions::Cursor> cursor_;
  std::string searchQuery_,searchDraft_;
  std::size_t searchCount_=0,searchCurrent_=0,newMessageCount_=0;
  bool lastHasGadgetsInProgress_=false;
  bool started_=false,transcript_=false,searchOpen_=false,
    terminalFocused_=true,poetryDesiredFocus_=true;
};
} // namespace batbox::interactive::conversation_shell
