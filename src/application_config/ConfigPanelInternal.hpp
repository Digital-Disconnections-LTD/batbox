#pragma once
#include "application_config/ConfigPanel.hpp"
#include "application_config/EndpointStore.hpp"
#include <map>
#include <optional>

namespace batbox::application_config {
using Endpoint=application::EndpointRecord;
using Done=interactive::CommandUi::Done;
using Ready=interactive::CommandUi::Ready;
using Error=interactive::CommandUi::Error;
enum class Page{Home,Detail,Models,Edit,Settings,Input,ConfirmDelete,Progress};
enum class Field{Label,BaseUrl,MessagesPath,ModelsPath,Token,KeyEnv,Context,Output,
  Model,Search,Compact,Headers,Subagents,ConcurrentSubagents};
std::string slug(std::string label);
std::string lower(std::string value);
std::size_t parsePositive(const std::string& value);
std::size_t movedIndex(std::size_t current,std::size_t begin,
  std::size_t visible,std::size_t total,std::size_t local);
std::string credential(const Endpoint& endpoint);
application_model::ModelProfile runtimeProfile(const std::string& id,
  const Endpoint& endpoint,const std::string& model);
class Panel final:public std::enable_shared_from_this<Panel> {
public:
  Panel(event_loop::Loop& owner,std::shared_ptr<application::Config> config,
    std::shared_ptr<application_model::LocalModelService> model,
    std::shared_ptr<app_state::AppStateStore> store,
    std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell,
    Done done,Ready ready,Error error,bool modelMode=false,std::string args={},
    std::shared_ptr<application_compaction_settings::Policy> policy={});
  void start();
private:
  void openInitial();
  int pageSize()const;
  void close(std::optional<std::string> message={});
  void publish(interactive::UiRequest request,
    std::function<void(const interactive::UiAction&)> action);
  void problem(std::exception_ptr error);
  std::string selectedProfile()const;
  void setSelectedProfile(const std::string& value);
  bool hasProfile(const std::map<std::string,Endpoint>& endpoints,
    const std::string& name)const;
  std::string fallbackProfile(const std::map<std::string,Endpoint>& next)const;
  void persist(std::map<std::string,Endpoint> next,std::string selection,
    Json settingEdits=Json::object());
  void showCurrent();
  void showHome();
  void showDetail();
  std::vector<std::string> filteredModels()const;
  void showModels();
  void refresh();
  void beginNew();
  void beginEdit();
  void showEdit();
  void showSettings();
  void confirmDelete();
  void input(Field field,Page back);
  void showInput();
  void backFromInput();
  void selfNewCancelled();
  void submitInput();
  std::shared_ptr<application_compaction_settings::Policy> policy_;
  event_loop::Loop& owner_;
  std::shared_ptr<application::Config> config_;
  std::shared_ptr<application_model::LocalModelService> model_;
  std::shared_ptr<app_state::AppStateStore> store_;
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell_;
  Done done_;Ready ready_;Error error_;
  std::map<std::string,Endpoint> endpoints_;
  Endpoint editing_;
  std::string endpointId_,search_,input_,notice_;
  Page page_=Page::Home,returnPage_=Page::Home,inputBack_=Page::Home;
  Field field_=Field::Label;
  std::size_t homeIndex_=0,homeBegin_=0,detailIndex_=0,modelIndex_=0,
    modelBegin_=0,editIndex_=0,editBegin_=0,settingsIndex_=0;
  bool active_=true,busy_=false,new_=false;
  bool modelMode_=false;
  std::string args_;
  std::optional<std::string> closeAfterSave_;
};
}
