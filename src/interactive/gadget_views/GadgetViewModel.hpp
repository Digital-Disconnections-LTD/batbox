#pragma once

#include "runtime/JsonValue.hpp"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Gadgets describe what their call or result means; the presentation layer
// decides how it looks. A view is a Json object whose "batboxView" member
// names its kind:
//   text  {lines,total,tone}             plain output, optionally truncated
//   lines {summary,lines,total,start}    a summary plus a numbered excerpt
//   list  {summary,items,total}          a summary plus the first results
//   shell {lines,total,errLines,errTotal,interrupted,note,background}
//   diff  {summary,hunks,total,added,removed}
//   error {lines,total}
// Every view carries "expanded" when the transcript asks for full output.
namespace batbox::interactive::gadget_views {

inline constexpr const char* viewKey="batboxView";
inline constexpr std::size_t collapsedLines=12;
inline constexpr std::size_t excerptLines=8;
inline constexpr std::size_t listItems=6;
inline constexpr std::size_t expandedLines=400;

bool isView(const Json& value);
std::string viewKind(const Json& value);

// Terminal-safe display text: CSI/OSC/DCS escapes and other control bytes are
// removed, carriage-return overwrites keep the final segment, and tabs expand
// to eight-column stops. Newlines are preserved.
std::string sanitize(std::string_view text);
// Splits on '\n' and drops one trailing empty line from a final newline.
std::vector<std::string> splitLines(std::string_view text);

Json textView(std::string_view text,std::string tone,bool expanded);
Json errorView(std::string_view text,bool expanded);
Json shellView(const Json& output,bool expanded);
Json readView(const Json& output,bool expanded);
Json listView(std::string summary,const std::vector<std::string>& items,
    std::size_t total,bool expanded);
Json globView(const Json& output,bool expanded);
Json grepView(const Json& output,bool expanded);
// Edit and overwriting Write results; structuredPatch holds unified hunks
// ({oldStart,newStart,lines:[" ctx","-old","+new"]}).
Json diffView(std::string_view verb,std::string_view path,
    const Json& structuredPatch,bool expanded);
// A Write that created a file: its first lines as additions.
Json createdFileView(std::string_view path,std::string_view content,bool expanded);
// Preview a pending Edit without reading the file: old and new strings as one
// hunk without file line numbers.
Json editPreview(std::string_view path,std::string_view before,std::string_view after,
    std::size_t maxLines);
Json writePreview(std::string_view path,std::string_view content,std::size_t maxLines);

// The most visible text in an arbitrary Gadget value: strings as they are,
// views by summary and lines, objects by their first text-like member.
std::string visibleText(const Json& value);
// Display path relative to the Roost (or "~" for the home directory).
std::string displayPath(std::string_view path,std::string_view cwd,
    std::string_view home);
std::string plural(std::size_t count,std::string_view one,std::string_view many);

} // namespace batbox::interactive::gadget_views
