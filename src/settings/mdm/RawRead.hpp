#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::settings::mdm {
enum class Platform {MacOS,Windows,Linux};
struct PlistPath {std::string path,label;};
struct RawExec {std::string stdoutText;int code=1;};
struct PlistStdout {std::string stdoutText,label;};
struct RawReadResult {
  std::optional<std::vector<PlistStdout>> plistStdouts;
  std::optional<std::string> hklmStdout,hkcuStdout;
};
struct Names {
  std::string macPreferenceDomain="com.batbox";
  std::string windowsHKLM="HKLM\\SOFTWARE\\Policies\\Batbox";
  std::string windowsHKCU="HKCU\\SOFTWARE\\Policies\\Batbox";
  std::string windowsValue="Settings";
  std::string plutilPath="/usr/bin/plutil";
};
struct RawReadServices {
  Platform platform=Platform::Linux;
  bool internalUser=false;
  Names names;
  std::function<std::string()> username,homedir;
  std::function<bool(const std::string&)> existsSync;
  std::function<event_loop::Deferred<RawExec>(
    const std::string&,const std::vector<std::string>&,double)> execFile;
};
std::vector<PlistPath> getMacOSPlistPaths(const RawReadServices& services);
class RawReader {
public:
  RawReader(event_loop::Loop& loop,RawReadServices services);
  event_loop::Deferred<RawReadResult> fireRawRead();
  void startMdmRawRead();
  std::optional<event_loop::Deferred<RawReadResult>> getMdmRawReadPromise()const;
private:
  event_loop::Loop& loop_;
  RawReadServices services_;
  std::optional<event_loop::Deferred<RawReadResult>> startup_;
};
RawReadServices indigenousRawReadServices(event_loop::Loop& loop,
  Platform platform,bool internalUser,Names names={});
Platform indigenousPlatform();
}
