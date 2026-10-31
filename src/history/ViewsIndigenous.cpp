#include "history/Views.hpp"
#include <utility>

namespace batbox::history {
HistoryViewImports indigenousHistoryViewImports(event_loop::Loop& loop,
    std::shared_ptr<HistoryState> shared,HistoryReaderImports reader,
    PastedContentImports pastes,
    std::function<std::string()> getProjectRoot,
    std::function<std::string()> getFlightId){
  HistoryViewImports imports;
  imports.loop=&loop;
  imports.getProjectRoot=std::move(getProjectRoot);
  imports.getFlightId=std::move(getFlightId);
  imports.makeLogEntryReader=[shared=std::move(shared),reader=std::move(reader)]{
    auto cursor=std::make_shared<HistoryLogReader>(shared,reader);
    return LogEntryCursor{
      [cursor]{return cursor->next();},
      [cursor]{return cursor->close();}};
  };
  imports.logEntryToHistoryEntry=[pastes=std::move(pastes)](
      const LogEntryRef& entry){
    return history::logEntryToHistoryEntry(*entry,pastes);
  };
  return imports;
}
}
