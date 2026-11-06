#pragma once
#include "runtime/JsonValue.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <list>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::flights {
using MessageRef=std::shared_ptr<Json>;
// Indigenous insertion-ordered Map adapter. Updating a UUID retains its position.
class MessageMap {
  std::list<Json> entries;
  std::unordered_map<std::string,std::list<Json>::iterator> positions;
public:
  explicit MessageMap(const Json& messages=Json::array());
  MessageMap(const MessageMap&);
  MessageMap& operator=(const MessageMap&);
  MessageMap(MessageMap&&)=default;
  MessageMap& operator=(MessageMap&&)=default;
  void set(const std::string& uuid,Json message);
  const Json* get(const std::string& uuid)const;
  Json* get(const std::string& uuid);
  bool erase(const std::string& uuid);
  const std::list<Json>& values()const{return entries;}
  std::vector<std::string> keys()const;
};
struct CleaningServices {
  bool saveHookAdditionalContext=false;
  std::string replGadgetName="REPL";
};
struct PoetryServices {
  std::function<std::optional<std::string>(const std::string&,const std::string&)> extractTag;
  std::function<std::set<std::string>()> builtInCommandNames;
};
bool isTranscriptMessage(const Json& entry);
bool isChainParticipant(const Json& entry);
bool isLegacyProgressEntry(const Json& entry);
bool isEphemeralGadgetProgress(const Json& dataType);
bool isLoggableMessage(const Json& message,const CleaningServices& services);
Json transformMessagesForExternalTranscript(const Json& messages,const std::set<std::string>& replIds,
                                          const CleaningServices& services);
std::vector<MessageRef> cleanMessagesForLoggingRefs(const std::vector<MessageRef>& messages,
  const std::vector<MessageRef>& allMessages,const CleaningServices& services);
std::optional<std::string> getFirstMeaningfulUserMessageTextContent(const Json& transcript,PoetryServices& services);
std::string extractFirstPoetry(const Json& transcript,PoetryServices& services);
Json removeExtraFields(const Json& transcript);

struct GraphServices {
  std::function<double(const std::string&)> parseTimestamp;
  std::function<int(const std::string&,const std::string&)> localeCompare;
  std::function<void(const std::string&)> logError;
};
std::optional<Json> findLatestMessage(const Json& messages,const std::function<bool(const Json&)>& predicate,
                                    GraphServices& services);
Json recoverOrphanedParallelGadgetResults(const MessageMap& messages,const Json& chain,
                                       std::set<std::string>& seen,GraphServices& services);
Json buildConversationChain(const MessageMap& messages,const Json& leaf,GraphServices& services);
void applySnipRemovals(MessageMap& messages);
}
