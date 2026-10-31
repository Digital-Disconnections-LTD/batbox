#include "application_history/History.hpp"
#include "history/ReaderIndigenous.hpp"
#include "history/WriterIndigenous.hpp"
#include "history/PasteStoreBinding.hpp"
#include "fs_operations/NodeFsBytesIndigenous.hpp"
#include "paste_store/Indigenous.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace batbox::application_history {
namespace {
bool envTruthy(const char* name){
  const auto* raw=std::getenv(name);
  if(!raw)return false;
  const std::string value=raw;
  return !value.empty()&&value!="0"&&value!="false"&&value!="no";
}
}
std::shared_ptr<Bundle> Bundle::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread()||inputs.stateDirectory.empty()||
     !inputs.projectRoot||!inputs.flightId)
    throw std::invalid_argument("History needs owner, state directory, project and session");
  if(!inputs.log)inputs.log=[](const std::string&){};
  if(!inputs.onError)inputs.onError=[](std::exception_ptr){};
  if(!inputs.skipPoetryHistory)
    inputs.skipPoetryHistory=[] {return envTruthy("BATBOX_SKIP_PROMPT_HISTORY");};
  auto result=std::shared_ptr<Bundle>(new Bundle(std::move(inputs)));
  result->initialize();
  return result;
}
void Bundle::initialize(){
  fs_=std::make_shared<fs_operations::IndigenousAsyncFsHost>(inputs_.owner);
  locks_=std::make_unique<locking::Manager>(inputs_.owner);
  cleanup_=std::make_unique<cleanup::Roster>(inputs_.owner);
  state_=std::make_shared<history::HistoryState>();
  auto configHome=[this]{return inputs_.stateDirectory;};
  auto pastes=paste_store::indigenousServices(inputs_.owner,fs_,configHome,
    inputs_.log);
  history::HistoryWriterServices writer;
  writer.loop=&inputs_.owner;
  writer.getConfigHomeDir=configHome;
  writer.getProjectRoot=inputs_.projectRoot;
  writer.getFlightId=inputs_.flightId;
  writer.jsonStringify=[](const Json& value){return value::stringify(value);};
  writer.logForDebugging=inputs_.log;
  writer.skipPoetryHistory=inputs_.skipPoetryHistory;
  history::PastedContentImports reader;
  history::bindPasteStore(writer,reader,std::move(pastes));
  writer=history::bindIndigenousHistoryWriterServices(std::move(writer),fs_,
    *locks_,*cleanup_);
  writer_=std::make_unique<history::HistoryWriter>(state_,std::move(writer));
  auto bytes=fs_operations::indigenousNodeFsBytesImports(inputs_.owner,fs_);
  auto imports=history::indigenousHistoryReaderImports(inputs_.owner,
    std::move(bytes),inputs_.flightId,configHome,
    [](const std::string& text){return value::parse(text);},inputs_.log);
  view_=history::indigenousHistoryViewImports(inputs_.owner,state_,
    std::move(imports),std::move(reader),inputs_.projectRoot,
    inputs_.flightId);
}
void Bundle::record(const std::string& display,const Json& pastedContents){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("History owner required");
  if(value::trim(display).empty())return;
  writer_->addToHistory(Json{{"display",display},
    {"pastedContents",pastedContents.is_object()?pastedContents:Json::object()}});
}
void Bundle::removeLast(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("History owner required");
  writer_->removeLastFromHistory();
}
event_loop::Deferred<event_loop::Unit> Bundle::flush(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("History owner required");
  return cleanup_->runCleanupFunctions();
}
std::shared_ptr<history::HistorySimpleCursor> Bundle::reader(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("History owner required");
  return std::make_shared<history::HistorySimpleCursor>(view_);
}

void Navigator::apply(std::size_t index,bool cursorToStart){
  if(index==0){
    inputs_.input=draft_;
  }else{
    const auto& entry=stash_.at(index-1);
    const auto& display=value::property(entry,"display");
    if(!display.is_string())return;
    inputs_.input.text=display.get<std::string>();
    const auto& pasted=value::property(entry,"pastedContents");
    inputs_.input.pastedContents=pasted.is_object()?pasted:Json::object();
    inputs_.input.caretUtf16=cursorToStart?0:
      value::stringLength(inputs_.input.text);
  }
  if(inputs_.redraw)inputs_.redraw();
}
void Navigator::pump(){
  if(reading_||index_==0)return;
  const auto chunkTarget=((index_+9)/10)*10;
  if(stash_.size()>=chunkTarget||exhausted_){
    if(index_>stash_.size())index_=stash_.size();
    if(index_==0)return;
    apply(index_,true);
    if(index_>=2&&!shownHint_){
      shownHint_=true;
      if(inputs_.notify)inputs_.notify("Ctrl+R searches prompt history");
    }
    return;
  }
  if(!reader_)reader_=inputs_.history->reader();
  reading_=true;
  const auto generation=generation_;
  auto weak=weak_from_this();
  reader_->next().then([weak,generation](const std::optional<Json>* entry,
      const std::exception_ptr& error){
    auto self=weak.lock();
    if(!self||self->generation_!=generation)return;
    self->reading_=false;
    if(error){
      self->exhausted_=true;
      if(self->inputs_.onError)self->inputs_.onError(error);
      return;
    }
    if(!entry->has_value()){
      self->exhausted_=true;
      self->pump();
      return;
    }
    const auto& display=value::property(**entry,"display");
    if(display.is_string()&&
       (!self->bashOnly_||display.get<std::string>().starts_with('!')))
      self->stash_.push_back(**entry);
    self->pump();
  });
}
void Navigator::up(){
  if(!inputs_.history||!inputs_.history->owner().isOwnerThread())
    throw std::logic_error("History navigation owner required");
  if(index_==0){
    draft_=inputs_.input;
    bashOnly_=inputs_.input.text.starts_with('!');
  }
  ++index_;
  pump();
}
bool Navigator::down(){
  if(!inputs_.history||!inputs_.history->owner().isOwnerThread())
    throw std::logic_error("History navigation owner required");
  if(index_==0)return true;
  --index_;
  if(index_==0){
    apply(0,false);
    return false;
  }
  if(index_<=stash_.size())apply(index_,false);
  return false;
}
void Navigator::reset(){
  if(!inputs_.history||!inputs_.history->owner().isOwnerThread())
    throw std::logic_error("History navigation owner required");
  ++generation_;
  if(reader_){
    auto closing=std::move(reader_);
    closing->close().then([closing,onError=inputs_.onError](
        const event_loop::Unit*,const std::exception_ptr& error){
      if(error&&onError)onError(error);
    });
  }
  stash_.clear();
  draft_=interactive::InputState{};
  index_=0;bashOnly_=reading_=exhausted_=false;
  ++searchGeneration_;
  closeSearchReader();
  searching_=searchReading_=searchFailed_=searchExhausted_=false;
  searchQuery_.clear();searchMatch_.reset();
  seenSearchPoetryEntries_.clear();
}
void Navigator::observeManualEdit(){
  if(index_>0)reset();
}
void Navigator::closeSearchReader(){
  if(!searchReader_)return;
  auto closing=std::move(searchReader_);
  closing->close().then([closing,onError=inputs_.onError](
      const event_loop::Unit*,const std::exception_ptr& error){
    if(error&&onError)onError(error);
  });
}
void Navigator::startSearch(){
  if(!inputs_.history||!inputs_.history->owner().isOwnerThread())
    throw std::logic_error("History search owner required");
  if(searching_)return;
  reset();
  searchOriginal_=inputs_.input;
  searching_=true;
  searchReader_=inputs_.history->reader();
  if(inputs_.redraw)inputs_.redraw();
}
void Navigator::setSearchQuery(std::string query){
  if(!searching_)return;
  ++searchGeneration_;
  closeSearchReader();
  searchQuery_=std::move(query);
  seenSearchPoetryEntries_.clear();
  searchReading_=searchFailed_=searchExhausted_=false;
  if(searchQuery_.empty()){
    searchMatch_.reset();
    inputs_.input=searchOriginal_;
    if(inputs_.redraw)inputs_.redraw();
    return;
  }
  searchReader_=inputs_.history->reader();
  pumpSearch();
}
void Navigator::pumpSearch(){
  if(!searching_||searchReading_||searchQuery_.empty()||
     searchExhausted_)return;
  if(!searchReader_)searchReader_=inputs_.history->reader();
  searchReading_=true;
  const auto generation=searchGeneration_;
  auto weak=weak_from_this();
  searchReader_->next().then([weak,generation](
      const std::optional<Json>* entry,const std::exception_ptr& error){
    auto self=weak.lock();
    if(!self||generation!=self->searchGeneration_||!self->searching_)return;
    self->searchReading_=false;
    if(error){
      self->searchFailed_=true;self->searchExhausted_=true;
      if(self->inputs_.onError)self->inputs_.onError(error);
      if(self->inputs_.redraw)self->inputs_.redraw();
      return;
    }
    if(!entry->has_value()){
      self->searchFailed_=true;self->searchExhausted_=true;
      if(self->inputs_.redraw)self->inputs_.redraw();
      return;
    }
    const auto& display=value::property(**entry,"display");
    if(display.is_string()){
      const auto text=display.get<std::string>();
      const auto position=text.rfind(self->searchQuery_);
      if(position!=std::string::npos&&
          self->seenSearchPoetryEntries_.insert(text).second){
        self->searchMatch_=**entry;
        self->searchFailed_=false;
        self->inputs_.input.text=text;
        const auto& pasted=value::property(**entry,"pastedContents");
        self->inputs_.input.pastedContents=pasted.is_object()?pasted:
          Json::object();
        self->inputs_.input.caretUtf16=value::stringLength(
          text.substr(0,position));
        if(self->inputs_.redraw)self->inputs_.redraw();
        return;
      }
    }
    self->pumpSearch();
  });
}
void Navigator::nextSearchMatch(){
  if(!searching_||searchQuery_.empty())return;
  pumpSearch();
}
void Navigator::acceptSearch(){
  if(!searching_)return;
  if(!searchMatch_)
    inputs_.input.pastedContents=searchOriginal_.pastedContents;
  reset();
  if(inputs_.redraw)inputs_.redraw();
}
void Navigator::cancelSearch(){
  if(!searching_)return;
  const auto original=searchOriginal_;
  reset();
  inputs_.input=original;
  if(inputs_.redraw)inputs_.redraw();
}
std::optional<std::string> Navigator::executeSearch(){
  if(!searching_)return {};
  std::optional<std::string> chosen;
  if(searchQuery_.empty())chosen=searchOriginal_.text;
  else if(searchMatch_){
    const auto& display=value::property(*searchMatch_,"display");
    if(display.is_string())chosen=display.get<std::string>();
  }
  if(chosen){
    if(searchQuery_.empty())inputs_.input=searchOriginal_;
    else inputs_.input.text=*chosen;
  }
  reset();
  if(inputs_.redraw)inputs_.redraw();
  return chosen;
}
} // namespace batbox::application_history
