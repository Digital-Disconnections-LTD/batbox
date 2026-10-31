#pragma once
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace batbox::diagnostics {
using SlowWriteData=std::variant<std::string,std::vector<unsigned char>>;
using SlowArgument=std::variant<std::monostate,Json,std::shared_ptr<Json>>;

struct SlowOperationConfig {
  bool featureEnabled=false;
  std::optional<std::string> thresholdEnvironment;
  std::string nodeEnvironment;
  bool internalUser=false;
};

struct SlowOperationImports {
  std::function<double()> performanceNow;
  std::function<std::optional<std::string>()> captureStack;
  std::function<void(const std::string&)> logForDebugging;
  std::function<void(const std::string&,double)> addSlowOperation;
  std::function<Json(const std::string&)> parseJson;
  std::function<std::string(const Json&,const std::optional<Json>&,
    const std::optional<Json>&)> stringifyJson;
  std::function<Json(const Json&,const std::optional<Json>&)> structuredCloneJson;
  std::function<Json(const Json&)> lodashCloneDeepJson;
  std::function<int(const std::string&,const std::optional<Json>&)> openSync;
  std::function<void(int,const SlowWriteData&,const std::optional<Json>&)> writeFdSync;
  std::function<void(int)> fsyncSync;
  std::function<void(int)> closeSync;
  std::function<void(const std::string&,const SlowWriteData&,
    const std::optional<Json>&)> writePathSync;
};

class SuppressedOperationError:public std::runtime_error {
public:
  SuppressedOperationError(std::exception_ptr error,std::exception_ptr suppressed);
  std::exception_ptr error()const{return error_;}
  std::exception_ptr suppressed()const{return suppressed_;}
private:
  std::exception_ptr error_,suppressed_;
};

double slowOperationThreshold(const SlowOperationConfig& config);
std::string slowOperationCallerFrame(const std::optional<std::string>& stack);
std::string buildSlowOperationDescription(const std::vector<std::string>& strings,
  const std::vector<std::optional<Json>>& values);

class SlowOperations {
public:
  SlowOperations(SlowOperationConfig config,SlowOperationImports imports);
  Json jsonParse(const std::string& text);
  std::string jsonStringify(const Json& value,
    std::optional<Json> replacer={},std::optional<Json> space={});
  Json clone(const Json& value,std::optional<Json> options={});
  Json cloneDeep(const Json& value);
  void writeFileSyncDeprecated(const std::string& filePath,
    const SlowWriteData& data,std::optional<Json> options={});
  std::function<void()> beginSlowLogging(std::vector<std::string> strings,
    std::vector<SlowArgument> values);
  double threshold()const{return threshold_;}
private:
  void finish(double started,const std::function<std::string()>& description,
    const std::optional<std::string>& stack);
  template<class Description,class Operation>auto timed(
      Description&& description,
      Operation&& operation)->decltype(operation()){
    if(!config_.featureEnabled)return operation();
    const auto started=imports_.performanceNow();
    const auto stack=imports_.captureStack();
    using Result=decltype(operation());
    if constexpr(std::is_void_v<Result>){
      try{operation();}
      catch(...){
        const auto original=std::current_exception();
        try{finish(started,description,stack);}
        catch(...){throw SuppressedOperationError(std::current_exception(),original);}
        std::rethrow_exception(original);
      }
      finish(started,description,stack);
    }else{
      Result result;
      try{result=operation();}
      catch(...){
        const auto original=std::current_exception();
        try{finish(started,description,stack);}
        catch(...){throw SuppressedOperationError(std::current_exception(),original);}
        std::rethrow_exception(original);
      }
      finish(started,description,stack);
      return result;
    }
  }
  SlowOperationConfig config_;
  SlowOperationImports imports_;
  double threshold_;
  bool isLogging_=false;
};
}
