#include "application_mcp_elicitation/DateTimeParser.hpp"
#include <array>
#include <cstdio>
#include <ctime>
#include <stdexcept>

namespace batbox::application_mcp_elicitation {
namespace {
constexpr auto parseError="Unable to parse date/time from input";
constexpr auto queryError=
  "Unable to parse date/time. Please enter in ISO 8601 format manually.";
bool digit(char ch){return ch>='0'&&ch<='9';}
void logFailure(const std::function<void(std::exception_ptr)>& log,
    std::exception_ptr error){
  if(log)try{log(error);}catch(...){}
}
std::tm utcTime(std::time_t value){
  std::tm output{};
#ifdef _WIN32
  if(gmtime_s(&output,&value)!=0)throw std::runtime_error("UTC clock unavailable");
#else
  if(!gmtime_r(&value,&output))throw std::runtime_error("UTC clock unavailable");
#endif
  return output;
}
std::tm localTime(std::time_t value){
  std::tm output{};
#ifdef _WIN32
  if(localtime_s(&output,&value)!=0)throw std::runtime_error("Local clock unavailable");
#else
  if(!localtime_r(&value,&output))throw std::runtime_error("Local clock unavailable");
#endif
  return output;
}
}
bool looksLikeISO8601(std::string_view input){
  const auto trimmed=value::trim(input);
  if(trimmed.size()<10)return false;
  for(const auto index:{0,1,2,3,5,6,8,9})
    if(!digit(trimmed[static_cast<std::size_t>(index)]))return false;
  return trimmed[4]=='-'&&trimmed[7]=='-'&&
    (trimmed.size()==10||trimmed[10]=='T');
}
DateTimeContext currentDateTimeContext(
    std::chrono::system_clock::time_point now){
  using namespace std::chrono;
  const auto second=floor<seconds>(now);
  const auto milliseconds=duration_cast<std::chrono::milliseconds>(now-second).count();
  const auto stamp=system_clock::to_time_t(second);
  const auto utc=utcTime(stamp),local=localTime(stamp);
  char clock[40];
  std::snprintf(clock,sizeof(clock),"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
    utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,
    utc.tm_hour,utc.tm_min,utc.tm_sec,static_cast<int>(milliseconds));
  const auto localDay=sys_days(year(local.tm_year+1900)/
    month(static_cast<unsigned>(local.tm_mon+1))/
    day(static_cast<unsigned>(local.tm_mday)));
  const auto utcDay=sys_days(year(utc.tm_year+1900)/
    month(static_cast<unsigned>(utc.tm_mon+1))/
    day(static_cast<unsigned>(utc.tm_mday)));
  const int offset=static_cast<int>((localDay-utcDay).count())*1440+
    (local.tm_hour-utc.tm_hour)*60+(local.tm_min-utc.tm_min);
  const auto absolute=offset<0?-offset:offset;
  char zone[7];
  std::snprintf(zone,sizeof(zone),"%c%02d:%02d",offset>=0?'+':'-',
    absolute/60,absolute%60);
  constexpr std::array weekdays={"Sunday","Monday","Tuesday",
    "Wednesday","Thursday","Friday","Saturday"};
  return {clock,zone,weekdays.at(static_cast<std::size_t>(local.tm_wday))};
}
DateTimePoetry buildDateTimePoetry(const std::string& input,
    const std::string& format,const DateTimeContext& context){
  DateTimePoetry result;
  result.systemPoetry=Json::array({
    "You are a date/time parser that converts natural language into ISO 8601 format.",
    "You MUST respond with ONLY the ISO 8601 formatted string, with no explanation or additional text.",
    "If the input is ambiguous, prefer future dates over past dates.",
    "For times without dates, use today's date.",
    "For dates without times, do not include a time component.",
    "If the input is incomplete or you cannot confidently parse it into a valid date, respond with exactly \"INVALID\" (nothing else).",
    "Examples of INVALID input: partial dates like \"2025-01-\", lone numbers like \"13\", gibberish.",
    "Examples of valid natural language: \"tomorrow\", \"next Monday\", \"jan 1st 2025\", \"in 2 hours\", \"yesterday\"."
  });
  const auto description=format=="date"?
    "YYYY-MM-DD (date only, no time)":
    "YYYY-MM-DDTHH:MM:SS"+context.timezone+
      " (full date-time with timezone)";
  result.userPoetry="Current context:\n- Current date and time: "+
    context.currentDateTime+" (UTC)\n- Local timezone: "+context.timezone+
    "\n- Day of week: "+context.dayOfWeek+"\n\nUser input: \""+input+
    "\"\n\nOutput format: "+description+
    "\n\nParse the user's input into ISO 8601 format. Return ONLY the formatted string, or \"INVALID\" if the input is incomplete or unparseable.";
  return result;
}
DateTimeParseResult parseDateTimeModelText(const std::string& text){
  const auto parsed=value::trim(text);
  if(parsed.empty()||parsed=="INVALID"||parsed.size()<4||
      !digit(parsed[0])||!digit(parsed[1])||
      !digit(parsed[2])||!digit(parsed[3]))
    return {false,{},parseError};
  return {true,parsed,{}};
}
event_loop::Deferred<DateTimeParseResult> parseNaturalLanguageDateTime(
    event_loop::Loop& owner,std::string input,std::string format,
    std::stop_token signal,DateTimeQuery query,
    std::function<void(std::exception_ptr)> logError){
  event_loop::Deferred<DateTimeParseResult> result(owner);
  try{
    if(!query)throw std::invalid_argument("Date/time model query is unavailable");
    const auto poetry=buildDateTimePoetry(input,format,currentDateTimeContext());
    query(poetry.systemPoetry,poetry.userPoetry,signal).then(
      [result,logError=std::move(logError)](
          const std::string* text,const std::exception_ptr& error){
        if(error){logFailure(logError,error);result.resolve({false,{},queryError});
          return;}
        try{result.resolve(parseDateTimeModelText(*text));}
        catch(...){logFailure(logError,std::current_exception());
          result.resolve({false,{},queryError});}
      });
  }catch(...){logFailure(logError,std::current_exception());
    result.resolve({false,{},queryError});}
  return result;
}
}
