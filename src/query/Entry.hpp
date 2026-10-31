#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace batbox::query {
using MessageRef = std::shared_ptr<Json>;
class Yield {
 public:
  Yield()=default;
  Yield(const Yield&)=default;
  Yield(Yield&&)=default;
  Yield& operator=(const Yield&)=default;
  Yield& operator=(Yield&&)=default;
  template<class F> requires(!std::is_same_v<std::remove_cvref_t<F>,Yield> &&
    std::is_invocable_r_v<void,F,const Json&>)
  Yield(F&& callback):byValue_(std::forward<F>(callback)){}
  static Yield withReferences(std::function<void(const MessageRef&)> callback){
    Yield result;result.byReference_=std::move(callback);return result;
  }
  void operator()(const Json& value) const {
    if(byReference_)byReference_(std::make_shared<Json>(value));
    else byValue_(value);
  }
  void operator()(const MessageRef& value) const {
    if(byReference_)byReference_(value);
    else byValue_(*value);
  }
  explicit operator bool() const {return static_cast<bool>(byReference_)||
    static_cast<bool>(byValue_);}
 private:
  std::function<void(const Json&)> byValue_;
  std::function<void(const MessageRef&)> byReference_;
};
using QueryLoop = std::function<Json(const Json& params,std::vector<std::string>& consumedCommandUuids,
                                     const Yield& yield)>;
using NotifyLifecycle = std::function<void(const std::string& uuid,const std::string& state)>;

Json query(const Json& params,const QueryLoop& queryLoop,
           const NotifyLifecycle& notifyCommandLifecycle,const Yield& yield);
} // namespace batbox::query
