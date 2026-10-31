#pragma once
#include "cli/structured/StructuredFlight.hpp"
#include "interactive/QueryTurn.hpp"
#include "query/AsyncEngine.hpp"
#include "messages/Messages.hpp"
#include "services/api/batbox.hpp"
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace batbox::application_cli {
enum class InputFormat {Text,StreamJson};
enum class OutputFormat {Text,Json,StreamJson};

struct Inputs {
  event_loop::Loop& owner;
  InputFormat input=InputFormat::Text;
  OutputFormat output=OutputFormat::Text;
  std::optional<std::string> poetry;
  bool replayUserMessages=false,verbose=false;
  std::function<std::string()> flightId,randomUUID;
  std::function<Json()> systemInit;
  messages::Services* messageServices=nullptr;
  std::function<double()> durationApiMs,totalCostUsd;
  std::function<Json()> modelUsage;
  std::function<event_loop::Deferred<bool>(const std::string&)>
    messageExists;
  std::function<std::optional<Json>(const std::string&)>
    acceptedUserMessage;
  std::function<bool(const std::string&,
    const std::optional<std::string>&)> submitPoetry;
  std::function<void()> interruptTurn;
  // Returns model-switch breadcrumbs after changing the model for
  // subsequent turns. The adapter replays the visible stdout breadcrumb.
  std::function<Json(const std::string& requestedModel)> setModel;
  std::function<Json()> mcpStatus;
  // A null enable flag reconnects; false disables; true enables and connects.
  std::function<event_loop::Deferred<Json>(const std::string& serverName,
    std::optional<bool> enabled)> manageMcp;
  std::function<void(const Json& maxThinkingTokens)> setMaxThinkingTokens;
  std::function<Json(const Json& request,const std::string& requestId)>
    setPermissionMode;
  // The headless exit code follows the final result.is_error, not an
  // earlier interrupted turn that a later successful turn supersedes.
  std::function<void(bool lastResultError)> requestExit;
  std::function<void(std::exception_ptr)> report;
  std::function<void(std::string_view)> writeStdout,writeStderr;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};

// Owner-loop headless flight. The stdin pump is a separate OS reader; model
// work and all transcript/output state remain on the application owner.
class Adapter final:public std::enable_shared_from_this<Adapter> {
public:
  static std::shared_ptr<Adapter> create(Inputs inputs);
  ~Adapter();
  // FlightStart's initialUserMessage is queued before argv/stdin input.
  void prependUserMessage(std::string message);
  void start();
  void stop();
  void onQueryEvent(const query::AsyncEngineStep& step);
  void onTurnRequested(const interactive::TurnRequest& request);
  void onTurnComplete(const interactive::TurnMessagesRef& messages,bool aborted,
    std::exception_ptr error={});
private:
  struct StdinPump;
  struct PendingPoetry {
    std::string value;
    std::optional<std::string> uuid;
    Json originalMessage;
    PendingPoetry(std::string text,std::optional<std::string> id={},
        Json original={}):
      value(std::move(text)),uuid(std::move(id)),
      originalMessage(std::move(original)){}
  };
  explicit Adapter(Inputs inputs):inputs_(std::move(inputs)){}
  void beginText();
  void beginStream();
  void nextStreamInput();
  void acceptStreamInput(Json message);
  void enqueueUser(Json message,std::string value,
    std::optional<std::string> uuid);
  void replayUser(const PendingPoetry& poetry,bool preserveOriginal=false,
    bool includeTimestamp=false);
  void submitNext();
  void emit(Json message);
  void fail(std::exception_ptr error);
  void finish();
  Inputs inputs_;
  std::shared_ptr<StdinPump> stdin_;
  std::shared_ptr<cli::structured::StructuredFlight> structured_;
  std::shared_ptr<std::function<void()>> textCollector_;
  event_loop::Loop::TimerHandle stdinTimeout_;
  std::deque<PendingPoetry> poetryEntries_;
  std::optional<PendingPoetry> pendingInitialReplay_;
  std::optional<std::string> hookInitialUserMessage_;
  std::size_t assistantTurns_=0;
  Json lastEvent_,lastStopReason_=nullptr;
  bool localTurn_=false;
  interactive::TurnMessagesRef localMessages_;
  std::string localResultText_;
  void emitLocalMessages();
  Json currentUsage_=EMPTY_USAGE,streamUsage_=EMPTY_USAGE,
    fallbackUsage_=EMPTY_USAGE;
  bool streamUsageSeen_=false;
  bool hasStreamUsage_=false;
  std::string currentStreamMessageId_;
  std::unordered_set<std::string> streamUsageIds_,fallbackUsageIds_;
  std::unordered_set<std::string> receivedUserUuids_;
  std::deque<std::string> receivedUserUuidsOrder_;
  Json fullMessages_=Json::array();
  Json lastEligibleOutput_=nullptr;
  std::int64_t turnStartMs_=0;
  bool started_=false,stopped_=false,inputClosed_=false,busy_=false;
  bool sawInput_=false,initialized_=false,inputPaused_=false;
  bool lastResultError_=false;
};
} // namespace batbox::application_cli
