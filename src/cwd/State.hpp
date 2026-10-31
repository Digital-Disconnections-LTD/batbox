#pragma once
#include <functional>
#include <string>

namespace batbox::cwd {
// The owner creates this state once at startup and supplies it to services
// that read or change the flight cwd.
struct State {
  std::string originalCwd;
  std::string projectRoot;
  std::string cwd;
};
struct InitialServices {
  std::function<std::string()> processCwd;
  std::function<std::string(const std::string&)> realpathSync;
};
State initialState(const InitialServices& services);
InitialServices indigenousInitialServices();
const std::string& getOriginalCwd(const State& state);
const std::string& getProjectRoot(const State& state);
const std::string& getCwdState(const State& state);
void setOriginalCwd(State& state,const std::string& value);
void setProjectRoot(State& state,const std::string& value);
void setCwdState(State& state,const std::string& value);
} // namespace batbox::cwd
