#include "mcp/Call.hpp"
#include "mcp/StdioClient.hpp"
#include "connectors/Abort.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}
int main(){
  batbox::mcp::Connection connection;
  connection.name="remote";connection.connected=true;connection.transportType="http";
  int cleared=0;connection.clearServerStash=[&](){++cleared;};
  connection.callGadget=[](const std::string&,const batbox::Json&,std::stop_token,
      const std::function<void(const batbox::Json&)>&)->batbox::mcp::GadgetCallResult{
    throw batbox::mcp::HttpStatusError(401,"unauthorized");};
  bool auth=false;
  try{(void)batbox::mcp::callMCPGadgetWithUrlElicitationRetry(connection,"tool",{}, {},{},{});}
  catch(const batbox::mcp::AuthError& error){auth=error.serverName=="remote";}
  require(auth&&cleared==0,"401 auth transition");
  connection.callGadget=[](const std::string&,const batbox::Json&,std::stop_token,
      const std::function<void(const batbox::Json&)>&)->batbox::mcp::GadgetCallResult{
    throw batbox::mcp::HttpStatusError(404,"{\"error\":{\"code\":-32001}}");};
  bool expired=false;
  try{(void)batbox::mcp::callMCPGadgetWithUrlElicitationRetry(connection,"tool",{}, {},{},{});}
  catch(const batbox::mcp::FlightExpiredError&){expired=true;}
  require(expired&&cleared==1,"expired session resets cache");
  connection.callGadget=[](const std::string&,const batbox::Json&,std::stop_token,
      const std::function<void(const batbox::Json&)>&)->batbox::mcp::GadgetCallResult{
    throw batbox::mcp::HttpStatusError(404,"not found");};
  bool raw404=false;
  try{(void)batbox::mcp::callMCPGadgetWithUrlElicitationRetry(connection,"tool",{}, {},{},{});}
  catch(const batbox::mcp::HttpStatusError&){raw404=true;}
  require(raw404&&cleared==1,"unrelated 404 remains HTTP error");
  connection.callGadget=[](const std::string&,const batbox::Json&,std::stop_token,
      const std::function<void(const batbox::Json&)>&)->batbox::mcp::GadgetCallResult{
    throw batbox::mcp::ProtocolError(-32000,"Connection closed");};
  expired=false;
  try{(void)batbox::mcp::callMCPGadgetWithUrlElicitationRetry(connection,"tool",{}, {},{},{});}
  catch(const batbox::mcp::FlightExpiredError&){expired=true;}
  require(expired&&cleared==2,"HTTP connection close resets cache");
  connection.transportType="stdio";
  bool rawProtocol=false;
  try{(void)batbox::mcp::callMCPGadgetWithUrlElicitationRetry(connection,"tool",{}, {},{},{});}
  catch(const batbox::mcp::ProtocolError&){rawProtocol=true;}
  require(rawProtocol&&cleared==2,"stdio connection close remains protocol error");
  connection.callGadget=[](const std::string&,const batbox::Json&,std::stop_token,
      const std::function<void(const batbox::Json&)>&)->batbox::mcp::GadgetCallResult{
    throw batbox::ConnectorAbortError();};
  require(batbox::mcp::callMCPGadgetWithUrlElicitationRetry(connection,"tool",{}, {},{},{}).aborted,
    "abort result");
  std::cout<<"native MCP auth, session recovery and abort transitions passed\n";
}
