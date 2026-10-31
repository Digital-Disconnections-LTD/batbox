#pragma once
#include "flights/Transcript.hpp"
namespace batbox::flights {
bool hasVisibleUserContent(const Json& message);
bool hasVisibleAssistantContent(const Json& message);
std::size_t countVisibleMessages(const Json& transcript);
Json buildFileHistorySnapshotChain(const Json& snapshots,const Json& conversation);
Json buildAttributionSnapshotChain(const Json& snapshots,const Json& conversation);
Json extractBatIdsFromMessages(const Json& messages);
Json extractPartnerTranscriptsFromMissions(const Json& missions);
std::optional<Json> getFlightIdFromLog(const Json& log);
bool isLiteLog(const Json& log);
}
