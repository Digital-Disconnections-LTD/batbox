#include "application_commands/MemoryPanel.hpp"
#include "application/Async.hpp"
#include "application_editor/IndigenousProcess.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
std::string text(const Json& row,const char* name){
  const auto& value=value::property(row,name);
  return value.is_string()?value.get<std::string>():std::string{};
}
class MemoryPanel final:public std::enable_shared_from_this<MemoryPanel> {
public:
  MemoryPanel(event_loop::Loop& owner,std::shared_ptr<application_memory::Provider> memory,
      std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell,
      interactive::CommandUi::Done done,interactive::CommandUi::Ready ready,
      interactive::CommandUi::Error error):owner_(owner),memory_(std::move(memory)),
      shell_(std::move(shell)),done_(std::move(done)),ready_(std::move(ready)),error_(std::move(error)){}
  void start(const std::string& args){
    if(args=="consolidate"){consolidate();return;}
    if(args=="refresh")memory_->refresh();
    load();
  }
private:
  void fail(std::exception_ptr error){
    if(!active_)return;
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception& issue){notice_=issue.what();}
    catch(...){notice_="Memory operation failed";}
    busy_=false;showList();
  }
  void finish(){
    active_=false;
    interactive::CommandCompletion completion;
    completion.options.display=interactive::DoneOptions::Display::Skip;
    done_(std::move(completion));
  }
  void load(){
    busy_=true;
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Progress;
    request.id="memory-loading";request.title="Memory";request.body="Reading memory sources…";
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel)self->finish();
    });
    memory_->listSources().then([self](const Json* rows,const std::exception_ptr& error){
      if(!self->active_)return;
      if(error){self->fail(error);return;}
      self->rows_=rows&&rows->is_array()?*rows:Json::array();
      self->memory_->status().then([self](const Json* state,const std::exception_ptr& failure){
        if(!self->active_)return;
        if(failure){self->fail(failure);return;}
        if(state)self->status_=*state;
        self->busy_=false;self->showList();
      });
    });
  }
  int pageSize()const{
    const auto shell=shell_();return std::clamp(shell?shell->viewState().rows-13:11,3,18);
  }
  void showList(){
    if(!active_)return;detail_=false;
    const auto total=rows_.size()+1;selected_=std::min(selected_,total-1);
    const auto capacity=static_cast<std::size_t>(pageSize());
    const auto begin=selected_>=capacity?selected_-capacity+1:0;
    visibleBegin_=begin;
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
    request.id="memory-sources";request.title="Memory";
    request.body="Auto memory: "+std::string(value::property(status_,"enabled")==true?"on":"off")+
      " · recall: "+(value::property(status_,"recallEnabled")==true?"on":"off")+
      " · consolidation: "+(value::property(status_,"running")==true?"running":
        value::property(status_,"autoDreamEnabled")==true?"automatic":"manual")+
      "\n↑/↓ select · Enter read · r refresh · Esc close";
    const auto lastResult=text(status_,"lastResult");
    if(!lastResult.empty()&&lastResult!="Not run")request.body+="\n"+lastResult;
    if(!notice_.empty())request.body+="\n"+notice_;
    for(auto index=begin;index<std::min(total,begin+capacity);++index){
      if(index==rows_.size()){request.choices.push_back("Consolidate recent sessions");continue;}
      const auto& row=rows_[index];
      auto label=text(row,"label");if(label.empty())label=text(row,"path");
      request.choices.push_back(text(row,"scope")+" · "+label+
        (value::property(row,"directory")==true?"/":value::property(row,"exists")==true?"":" (create)"));
    }
    request.selectedIndex=selected_-begin;
    auto self=shared_from_this();ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel||action.kind==interactive::UiAction::Kind::Back){self->finish();return;}
      if(action.kind==interactive::UiAction::Kind::TextChange&&action.text=="r"){
        self->memory_->refresh();self->load();return;
      }
      if(action.kind==interactive::UiAction::Kind::MoveSelection){
        if(action.text=="up"&&self->selected_>0)--self->selected_;
        else if(action.text=="down")self->selected_=std::min(self->selected_+1,self->rows_.size());
        else if(action.text!="up")self->selected_=std::min(self->visibleBegin_+action.index,self->rows_.size());
        self->showList();return;
      }
      if(action.kind!=interactive::UiAction::Kind::Activate)return;
      if(self->selected_==self->rows_.size())self->consolidate();
      else self->openFile(self->rows_[self->selected_]);
    });
  }
  void openFile(Json row){
    current_=std::move(row);page_=0;fileSelected_=0;content_.clear();detail_=true;
    if(value::property(current_,"directory")==true||value::property(current_,"exists")!=true){showFile();return;}
    auto self=shared_from_this();
    memory_->read(text(current_,"path")).then([self](const std::string* contents,const std::exception_ptr& error){
      if(!self->active_)return;
      if(error){self->fail(error);return;}
      self->content_=contents?*contents:"";self->showFile();
    });
  }
  void showFile(){
    if(!active_)return;detail_=true;
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
    request.id="memory-file";request.title=text(current_,"label");
    request.body=text(current_,"path");
    const auto description=text(current_,"description");
    if(!description.empty())request.body+="\n"+description;
    const bool directory=value::property(current_,"directory")==true;
    if(directory){request.body+="\nChoose a memory file in this folder from the source list.";request.choices={"Back"};}
    else{
      std::vector<std::string> lines;std::size_t at=0;
      while(at<content_.size()){
        const auto end=content_.find('\n',at);lines.push_back(content_.substr(at,end==std::string::npos?end:end-at));
        if(end==std::string::npos)break;at=end+1;
      }
      const auto capacity=static_cast<std::size_t>(std::max(2,pageSize()-4));
      const auto pages=std::max<std::size_t>(1,(lines.size()+capacity-1)/capacity);
      page_=std::min(page_,pages-1);pages_=pages;
      request.body+="\n";
      if(lines.empty())request.body+=value::property(current_,"exists")==true?"(empty file)":"Created when you choose Edit.";
      for(auto index=page_*capacity;index<std::min(lines.size(),(page_+1)*capacity);++index)
        request.body+="\n"+lines[index];
      request.body+="\nPage "+std::to_string(page_+1)+"/"+std::to_string(pages)+" · e edit · n/p page";
      request.choices={"Edit in external editor","Next page","Previous page","Back"};
    }
    request.selectedIndex=directory?0:fileSelected_;
    auto self=shared_from_this();ready_(std::move(request),[self,directory](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel||action.kind==interactive::UiAction::Kind::Back){self->showList();return;}
      if(directory){if(action.kind==interactive::UiAction::Kind::Activate)self->showList();return;}
      if(action.kind==interactive::UiAction::Kind::MoveSelection){
        self->fileSelected_=std::min<std::size_t>(3,action.index);self->showFile();return;
      }
      if(action.kind==interactive::UiAction::Kind::TextChange){
        if(action.text=="e")self->edit();
        else if(action.text=="n"){self->page_=std::min(self->page_+1,self->pages_-1);self->showFile();}
        else if(action.text=="p"){if(self->page_>0)--self->page_;self->showFile();}
      }else if(action.kind==interactive::UiAction::Kind::Activate){
        if(action.index==0)self->edit();
        else if(action.index==1){self->page_=std::min(self->page_+1,self->pages_-1);self->showFile();}
        else if(action.index==2){if(self->page_>0)--self->page_;self->showFile();}
        else self->showList();
      }
    });
  }
  void edit(){
    if(busy_)return;
    const auto editor=application_editor::indigenous::externalEditor();
    if(!editor){notice_="Set VISUAL or EDITOR to edit memory files.";showList();return;}
    busy_=true;const auto path=text(current_,"path");auto self=shared_from_this();
    memory_->prepareForEdit(path).then([self,path,editor=*editor](const event_loop::Unit*,const std::exception_ptr& error){
      if(!self->active_)return;
      if(error){self->fail(error);return;}
      self->owner_.queueNextTurn([self,path,editor]{
        const auto shell=self->shell_();
        if(!self->active_||!shell){self->busy_=false;return;}
        try{
          shell->driver().suspend();
          application::runInWorker<application_editor::indigenous::ProcessResult>(self->owner_,[editor,path]{
            return application_editor::indigenous::runInherited(editor,path);
          }).then([self,shell,path](const application_editor::indigenous::ProcessResult* result,const std::exception_ptr& failure){
            try{shell->driver().resume();}catch(...){self->fail(std::current_exception());return;}
            self->busy_=false;if(!self->active_)return;
            if(failure){self->fail(failure);return;}
            if(result&&(result->error||result->exitCode!=0)){
              self->notice_=result->error.value_or("Editor exited with code "+std::to_string(result->exitCode));self->showList();return;
            }
            self->memory_->refresh();self->notice_="Memory refreshed after editing "+path;self->load();
          });
        }catch(...){try{shell->driver().resume();}catch(...){}self->fail(std::current_exception());}
      });
    });
  }
  void consolidate(){
    if(busy_)return;busy_=true;
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Progress;
    request.id="memory-consolidating";request.title="Memory consolidation";
    request.body="Reading recent sessions and updating durable memory…\nEsc closes this panel; consolidation continues.";
    auto self=shared_from_this();ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel)self->finish();
    });
    memory_->consolidate(Json::object()).then([self](const event_loop::Unit*,const std::exception_ptr& error){
      self->busy_=false;if(!self->active_)return;
      if(error){self->fail(error);return;}
      self->notice_="Memory consolidation finished";self->load();
    });
  }
  event_loop::Loop& owner_;
  std::shared_ptr<application_memory::Provider> memory_;
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell_;
  interactive::CommandUi::Done done_;interactive::CommandUi::Ready ready_;interactive::CommandUi::Error error_;
  Json rows_=Json::array(),status_=Json::object(),current_;
  std::string content_,notice_;
  std::size_t selected_=0,visibleBegin_=0,page_=0,pages_=1,fileSelected_=0;
  bool active_=true,busy_=false,detail_=false;
};
}
user_input::CommandEntry makeMemoryCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<application_memory::Provider> memory,
    std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell){
  Json descriptor={{"type","local-jsx"},{"name","memory"},
    {"description","Read, edit and consolidate project and agent memory"},
    {"argumentHint","[refresh|consolidate]"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [&owner,memory=std::move(memory),shell=std::move(shell)](const std::string& args){
      return [&owner,memory,shell,args](auto done,auto ready,auto error){
        auto panel=std::make_shared<MemoryPanel>(owner,memory,shell,std::move(done),std::move(ready),std::move(error));
        panel->start(value::trim(args));
      };
    });
}
} // namespace batbox::application_commands
