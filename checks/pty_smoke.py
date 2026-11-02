"""Shared fixtures for the POSIX terminal smoke checks.

Every wait is an explicit condition with its own deadline, terminal input is
written without blocking so the terminal's output is drained while large input
is still queued.
"""
import errno
import fcntl
import os
import pty
import select
import signal
import struct
import termios
import time

STEP_SECONDS = 30
_WRITE_CHUNK = 512


class PtySession:
    """A child process on a pseudo-terminal, with its output kept in memory."""

    def __init__(self, argv, env, rows=24, columns=100):
        self.output = bytearray()
        self.status = None
        self.pid, self.master = pty.fork()
        if self.pid == 0:
            try:
                fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))
                os.execve(argv[0], argv, env)
            finally:
                os._exit(127)

    def _read_available(self, timeout):
        if not select.select([self.master], [], [], timeout)[0]:
            return False
        try:
            data = os.read(self.master, 65536)
        except OSError as error:
            if error.errno != errno.EIO:
                raise
            return False
        self.output.extend(data)
        return bool(data)

    def _reap(self):
        if self.status is None:
            finished, result = os.waitpid(self.pid, os.WNOHANG)
            if finished:
                self.status = result
                # Keep what the terminal wrote just before it exited.
                while self._read_available(0):
                    pass

    def drain(self, seconds):
        """Read terminal output for up to ``seconds`` or until the child exits."""
        deadline = time.monotonic() + seconds
        while self.status is None:
            self._read_available(min(0.02, max(0.0, deadline - time.monotonic())))
            self._reap()
            if time.monotonic() >= deadline:
                break

    def wait_for(self, condition, label, seconds=STEP_SECONDS, tail=1500):
        deadline = time.monotonic() + seconds
        while not condition() and self.status is None and time.monotonic() < deadline:
            self.drain(0.05)
        if not condition():
            raise RuntimeError(f"Timed out after {seconds}s waiting for {label}; "
                               f"exit={self.status}; terminal_tail={bytes(self.output[-tail:])!r}")

    def wait_for_exit(self, seconds=STEP_SECONDS):
        self.wait_for(lambda: self.status is not None, "clean exit", seconds)
        return os.waitstatus_to_exitcode(self.status)

    def write(self, data, seconds=STEP_SECONDS):
        """Queue all of ``data`` as terminal input, reading output meanwhile.

        A blocking write can deadlock: the child stops reading input while it
        waits for us to read its output, and we stop reading while the input
        queue is full.
        """
        view = memoryview(data)
        deadline = time.monotonic() + seconds
        flags = fcntl.fcntl(self.master, fcntl.F_GETFL)
        fcntl.fcntl(self.master, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        try:
            while view:
                self._reap()
                if self.status is not None:
                    raise RuntimeError(f"Terminal exited with {self.status} while "
                                       f"{len(view)} input bytes were pending")
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"Terminal accepted no input for {seconds}s; "
                                       f"{len(view)} bytes pending; "
                                       f"terminal_tail={bytes(self.output[-1500:])!r}")
                readable, writable, _ = select.select([self.master], [self.master], [], 0.02)
                if readable:
                    self._read_available(0)
                if writable:
                    try:
                        view = view[os.write(self.master, view[:_WRITE_CHUNK]):]
                    except BlockingIOError:
                        pass
        finally:
            fcntl.fcntl(self.master, fcntl.F_SETFL, flags)

    def send(self, data, settle=0.1):
        """Write input, then keep draining output for a short settle period."""
        self.write(data)
        self.drain(settle)

    def resize(self, rows, columns):
        fcntl.ioctl(self.master, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))
        os.kill(self.pid, signal.SIGWINCH)

    def close(self):
        if self.status is None:
            try:
                os.kill(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(self.pid, 0)
        os.close(self.master)


def child_environment(config, **extra):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("BATBOX_") and key not in {"TMUX", "STY", "SSH_CONNECTION"}}
    env.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config), **extra)
    return env
