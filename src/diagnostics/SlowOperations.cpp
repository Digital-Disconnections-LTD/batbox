#include "diagnostics/SlowOperations.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <regex>
#include <sstream>
#include <utility>

namespace batbox::diagnostics {
namespace {
double numberFromEnvironment(const std::string& text){
  const auto trimmed=value::trim(text);
  if(trimmed.empty())return 0;
  if(trimmed=="Infinity"||trimmed=="+Infinity")
    return std::numeric_limits<double>::infinity();
  if(trimmed=="-Infinity")return -std::numeric_limits<double>::infinity();
  std::size_t offset=0;
  unsigned base=0;
  if(trimmed.size()>2&&trimmed[0]=='0'){
    if(trimmed[1]=='x'||trimmed[1]=='X')base=16;
    if(trimmed[1]=='b'||trimmed[1]=='B')base=2;
    if(trimmed[1]=='o'||trimmed[1]=='O')base=8;
  }
  if(base){
    double result=0;offset=2;
    if(offset==trimmed.size())return std::numeric_limits<double>::quiet_NaN();
    for(;offset<trimmed.size();++offset){
      const auto c=trimmed[offset];
      const auto digit=c>='0'&&c<='9'?c-'0':
        c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
      if(digit<0||static_cast<unsigned>(digit)>=base)
        return std::numeric_limits<double>::quiet_NaN();
      result=result*base+digit;
    }
    return result;
  }
  char* end=nullptr;
  const auto result=std::strtod(trimmed.c_str(),&end);
  return end==trimmed.c_str()+trimmed.size()?result:
    std::numeric_limits<double>::quiet_NaN();
}

std::string valueText(const std::optional<Json>& value){
  if(!value)return "undefined";
  if(value->is_array())return "Array["+std::to_string(value->size())+"]";
  if(value->is_object())return "Object{"+std::to_string(value->size())+" keys}";
  if(value->is_string()){
    const auto text=value->get<std::string>();
    return value::stringLength(text)>80?value::slice(text,0,80)+"…":text;
  }
  if(value->is_null())return "null";
  if(value->is_boolean())return value->get<bool>()?"true":"false";
  return value::numberToString(value->get<double>());
}

std::string oneDecimal(double value){
  if(!std::isfinite(value))return value::numberToString(value);
  const auto tenths=static_cast<long long>(std::floor(
    static_cast<long double>(value)*10.0L+0.5L));
  return std::to_string(tenths/10)+"."+std::to_string(tenths%10);
}
}

SuppressedOperationError::SuppressedOperationError(
    std::exception_ptr error,std::exception_ptr suppressed):
  std::runtime_error("An error was suppressed during disposal."),
  error_(std::move(error)),suppressed_(std::move(suppressed)){}

double slowOperationThreshold(const SlowOperationConfig& config){
  if(config.thresholdEnvironment){
    const auto parsed=numberFromEnvironment(*config.thresholdEnvironment);
    if(!std::isnan(parsed)&&parsed>=0)return parsed;
  }
  if(config.nodeEnvironment=="development")return 20;
  if(config.internalUser)return 300;
  return std::numeric_limits<double>::infinity();
}

std::string slowOperationCallerFrame(const std::optional<std::string>& stack){
  if(!stack||stack->empty())return "";
  const std::regex frame(R"(([^/\\]+?):(\d+):\d+\)?$)");
  std::istringstream lines(*stack);
  std::string line;
  while(std::getline(lines,line)){
    if(line.find("slowOperations")!=std::string::npos)continue;
    std::smatch match;
    if(std::regex_search(line,match,frame))return " @ "+match[1].str()+":"+match[2].str();
  }
  return "";
}

std::string buildSlowOperationDescription(const std::vector<std::string>& strings,
    const std::vector<std::optional<Json>>& values){
  std::string description;
  for(std::size_t i=0;i<strings.size();++i){
    description+=strings[i];
    if(i<values.size())description+=valueText(values[i]);
  }
  return description;
}

SlowOperations::SlowOperations(SlowOperationConfig config,SlowOperationImports imports):
    config_(std::move(config)),imports_(std::move(imports)),
    threshold_(slowOperationThreshold(config_)){
  if(!imports_.parseJson||
      (config_.featureEnabled&&(!imports_.performanceNow||!imports_.captureStack||
        !imports_.logForDebugging||!imports_.addSlowOperation)))
    throw std::invalid_argument("Slow operation imports are required");
}

void SlowOperations::finish(double started,
    const std::function<std::string()>& describe,
    const std::optional<std::string>& stack){
  const auto duration=imports_.performanceNow()-started;
  if(duration>threshold_&&!isLogging_){
    isLogging_=true;
    try{
      const auto description=describe()+slowOperationCallerFrame(stack);
      imports_.logForDebugging("[SLOW OPERATION DETECTED] "+description+
        " ("+oneDecimal(duration)+"ms)");
      imports_.addSlowOperation(description,duration);
    }catch(...){isLogging_=false;throw;}
    isLogging_=false;
  }
}

std::function<void()> SlowOperations::beginSlowLogging(
    std::vector<std::string> strings,std::vector<SlowArgument> values){
  if(!config_.featureEnabled)return []{};
  const auto started=imports_.performanceNow();
  const auto stack=imports_.captureStack();
  return [this,started,stack,strings=std::move(strings),
      values=std::move(values)]{
    finish(started,[&]{
      std::vector<std::optional<Json>> current;
      current.reserve(values.size());
      for(const auto& value:values){
        if(std::holds_alternative<std::monostate>(value))current.emplace_back();
        else if(std::holds_alternative<Json>(value))current.emplace_back(
          std::get<Json>(value));
        else{
          const auto& pointer=std::get<std::shared_ptr<Json>>(value);
          if(pointer)current.emplace_back(*pointer);
          else current.emplace_back();
        }
      }
      return buildSlowOperationDescription(strings,current);
    },stack);
  };
}

Json SlowOperations::jsonParse(const std::string& text){
  return timed([&]{return buildSlowOperationDescription(
      {"JSON.parse(",")"},{Json(text)});},
    [this,&text]{return imports_.parseJson(text);});
}

std::string SlowOperations::jsonStringify(const Json& value,
    std::optional<Json> replacer,std::optional<Json> space){
  if(!imports_.stringifyJson)
    throw std::invalid_argument("JSON.stringify import is required");
  return timed([&]{return buildSlowOperationDescription(
      {"JSON.stringify(",")"},{value});},
    [this,&value,&replacer,&space]{
      return imports_.stringifyJson(value,replacer,space);
    });
}

Json SlowOperations::clone(const Json& value,std::optional<Json> options){
  if(!imports_.structuredCloneJson)
    throw std::invalid_argument("structuredClone import is required");
  return timed([&]{return buildSlowOperationDescription(
      {"structuredClone(",")"},{value});},
    [this,&value,&options]{return imports_.structuredCloneJson(value,options);});
}

Json SlowOperations::cloneDeep(const Json& value){
  if(!imports_.lodashCloneDeepJson)
    throw std::invalid_argument("lodash cloneDeep import is required");
  return timed([&]{return buildSlowOperationDescription(
      {"cloneDeep(",")"},{value});},
    [this,&value]{return imports_.lodashCloneDeepJson(value);});
}

void SlowOperations::writeFileSyncDeprecated(const std::string& filePath,
    const SlowWriteData& data,std::optional<Json> options){
  if(!imports_.openSync||!imports_.writeFdSync||!imports_.fsyncSync||
      !imports_.closeSync||!imports_.writePathSync)
    throw std::invalid_argument("Synchronous file imports are required");
  timed([&]{
    Json descriptionData=Json::object();
    if(std::holds_alternative<std::string>(data))
      descriptionData=std::get<std::string>(data);
    else{
      const auto& bytes=std::get<std::vector<unsigned char>>(data);
      for(std::size_t i=0;i<bytes.size();++i)
        descriptionData[std::to_string(i)]=bytes[i];
    }
    return buildSlowOperationDescription(
      {"fs.writeFileSync(",", ",")"},{Json(filePath),descriptionData});
  },[&]{
    const bool needsFlush=options&&options->is_object()&&
      options->contains("flush")&&(*options)["flush"].is_boolean()&&
      (*options)["flush"].get<bool>();
    if(needsFlush){
      std::optional<Json> encoding,mode;
      if(options->contains("encoding")&&!(*options)["encoding"].is_null())
        encoding=(*options)["encoding"];
      if(options->contains("mode"))mode=(*options)["mode"];
      std::optional<int> fd;
      try{
        fd=imports_.openSync(filePath,mode);
        imports_.writeFdSync(*fd,data,encoding);
        imports_.fsyncSync(*fd);
      }catch(...){
        const auto original=std::current_exception();
        if(fd)imports_.closeSync(*fd);
        std::rethrow_exception(original);
      }
      if(fd)imports_.closeSync(*fd);
    }else imports_.writePathSync(filePath,data,options);
  });
}
}
