#include "application_skills/ShellExpansion.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/JsonValue.hpp"
#include "skills/process/PoetryCommand.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include <exception>
#include <future>
#include <mutex>
#include <regex>
#include <stdexcept>

namespace batbox::application_skills {
namespace {
bool whitespaceBefore(const std::string& text,std::size_t position){
  if(position==0)return true;
  auto at=position-1;
  while(at>0&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;
  return value::trim(std::string_view(text).substr(at,position-at)).empty();
}
[[noreturn]] void rethrowFormatted(const std::string& pattern){
  try{throw;}
  catch(const skills::process::MalformedCommandError&){throw;}
  catch(const bash_gadget::ShellError& error){
    if(error.interrupted)throw skills::process::MalformedCommandError(
      "Shell command interrupted for pattern \""+pattern+
      "\": [Command interrupted]");
    throw skills::process::MalformedCommandError(
      "Shell command failed for pattern \""+pattern+"\": "+
      formatPoetryShellOutput(error.stdoutText,error.stderrText));
  }
  catch(const std::exception& error){
    throw skills::process::MalformedCommandError(
      std::string("[Error]\n")+error.what());
  }
  catch(...){throw skills::process::MalformedCommandError("[Error]\nUnknown error");}
}
}
std::string formatPoetryShellOutput(const std::string& stdoutText,
    const std::string& stderrText){
  std::string output;
  const auto stdoutTrimmed=value::trim(stdoutText);
  const auto stderrTrimmed=value::trim(stderrText);
  if(!stdoutTrimmed.empty())output=stdoutTrimmed;
  if(!stderrTrimmed.empty()){
    if(!output.empty())output+='\n';
    output+="[stderr]\n"+stderrTrimmed;
  }
  return output;
}
std::vector<PoetryShellMatch> parsePoetryShellMatches(const std::string& text){
  std::vector<PoetryShellMatch> matches;
  static const std::regex block(R"(```!\s*\n?([\s\S]*?)\n?```)",
    std::regex::ECMAScript);
  for(std::sregex_iterator it(text.begin(),text.end(),block),end;it!=end;++it)
    matches.push_back({it->str(0),value::trim(it->str(1))});
  if(text.find("!`")==std::string::npos)return matches;
  for(std::size_t at=0;(at=text.find("!`",at))!=std::string::npos;){
    const auto close=text.find('`',at+2);
    if(close!=std::string::npos&&close>at+2&&whitespaceBefore(text,at)){
      matches.push_back({text.substr(at,close-at+1),
        value::trim(std::string_view(text).substr(at+2,close-at-2))});
      at=close+1;
    }else at+=2;
  }
  return matches;
}
std::string expandPoetryShell(const std::string& text,
    const std::string& slashCommandName,const PoetryShellRun& run){
  if(!run)throw std::invalid_argument("Skill shell runner is unavailable");
  const auto matches=parsePoetryShellMatches(text);
  if(matches.empty())return text;
  std::string result=text;
  std::mutex mutex;
  std::exception_ptr firstError;
  std::vector<std::future<void>> pending;
  pending.reserve(matches.size());
  for(const auto& match:matches){
    if(match.command.empty())continue;
    pending.push_back(runtime::contextAsync(std::launch::async,
      [&,match]{
        try{
          const auto replacement=run(match.command,match.pattern);
          std::lock_guard lock(mutex);
          if(const auto at=result.find(match.pattern);at!=std::string::npos)
            result.replace(at,match.pattern.size(),replacement);
        }catch(...){
          try{rethrowFormatted(match.pattern);}catch(...){
            std::lock_guard lock(mutex);
            if(!firstError)firstError=std::current_exception();
          }
        }
      }));
  }
  for(auto& mission:pending)mission.get();
  if(firstError)std::rethrow_exception(firstError);
  (void)slashCommandName;
  return result;
}
}
