#include "terminal_ftxui/Driver.hpp"
#include <ftxui/dom/elements.hpp>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <poll.h>
#include <signal.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using batbox::event_loop::Loop;
using batbox::terminal_ftxui::Callbacks;
using batbox::terminal_ftxui::Driver;
using batbox::terminal_ftxui::Options;

static void child(int reportFd) {
  int rendered = 0, burstFrames = 0, renderedBeforeLate = 0, renderedAfterLate = 0;
  try {
    Loop owner;
    Options options;
    auto driver = std::make_shared<Driver>(owner, options, Callbacks{
      .render = [&] { ++rendered; return ftxui::text("Batbox frame cap probe"); },
      .onRawInputBytes = [](std::string_view) {},
      .onEvent = [](const ftxui::Event&) {},
      .onResize = [](ftxui::Dimensions) {},
      .renderPlain = {},
      .writePlain = {},
      .onExit = [] {},
      .onFrameCommitted = [] {},
    });
    driver->start();
    // A stream asks for a redraw every two milliseconds for one second.
    int startFrames = 0;
    bool streaming = false;
    std::function<void()> tick;
    Loop::TimerHandle ticker;
    tick = [&] {
      if (!streaming) return;
      driver->requestRedraw();
      ticker = owner.setTimeout(2, tick);
    };
    auto begin = owner.setTimeout(200, [&] {
      startFrames = rendered; streaming = true; tick();
    });
    auto end = owner.setTimeout(1200, [&] {
      streaming = false; ticker.cancel(); burstFrames = rendered - startFrames;
    });
    // After a quiet spell a single request still paints within one pump.
    auto late = owner.setTimeout(1500, [&] {
      renderedBeforeLate = rendered; driver->requestRedraw();
    });
    auto check = owner.setTimeout(1540, [&] { renderedAfterLate = rendered; });
    auto stop = owner.setTimeout(1700, [&] { driver->stop(); });
    owner.run();
    begin.cancel(); end.cancel(); late.cancel(); check.cancel(); stop.cancel();
  } catch (const std::exception& error) {
    std::string message = std::string("ERR ") + error.what() + "\n";
    (void)!write(reportFd, message.data(), message.size());
    _exit(2);
  }
  // Unthrottled, the pump paints about 80 frames in that second.
  const bool ok = burstFrames >= 25 && burstFrames <= 60 &&
      renderedAfterLate > renderedBeforeLate;
  const std::string report = std::string(ok ? "PASS " : "FAIL ") +
      "burstFrames=" + std::to_string(burstFrames) +
      " lateFrame=" + std::to_string(renderedAfterLate - renderedBeforeLate) + "\n";
  (void)!write(reportFd, report.data(), report.size());
  _exit(ok ? 0 : 1);
}

int main() {
  int report[2];
  if (pipe(report) != 0) { std::cerr << strerror(errno) << '\n'; return 2; }
  int master = -1;
  winsize size{};
  size.ws_row = 24;
  size.ws_col = 80;
  const pid_t pid = forkpty(&master, nullptr, nullptr, &size);
  if (pid < 0) { std::cerr << strerror(errno) << '\n'; return 2; }
  if (pid == 0) { close(report[0]); child(report[1]); }
  close(report[1]);
  const auto started = std::chrono::steady_clock::now();
  std::string diagnostic;
  for (;;) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    pollfd fds[2] = {{master, POLLIN, 0}, {report[0], POLLIN, 0}};
    const int ready = poll(fds, 2, 25);
    if (ready < 0 && errno != EINTR) { std::cerr << strerror(errno) << '\n'; break; }
    if (fds[0].revents & POLLIN) {
      char bytes[65536];
      (void)!read(master, bytes, sizeof(bytes));
    }
    if (fds[1].revents & POLLIN) {
      char bytes[512];
      const auto n = read(report[0], bytes, sizeof(bytes));
      if (n > 0) diagnostic.append(bytes, static_cast<std::size_t>(n));
    }
    if (diagnostic.find('\n') != std::string::npos) break;
    if (elapsed > 10000) { ::kill(pid, SIGKILL); break; }
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) { std::cerr << strerror(errno) << '\n'; return 2; }
  close(master);
  close(report[0]);
  std::cout << diagnostic;
  if (diagnostic.rfind("PASS ", 0) != 0) return 2;
  return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
}
