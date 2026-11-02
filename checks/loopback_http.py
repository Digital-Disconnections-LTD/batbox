"""Loopback HTTP fixture server shared by the smoke checks."""
import http.server
import socketserver


class LoopbackHTTPServer(http.server.ThreadingHTTPServer):
    """ThreadingHTTPServer without HTTPServer.server_bind's socket.getfqdn().

    The reverse lookup of 127.0.0.1 has taken more than 30 seconds on hosted
    macOS runners, which is longer than some of these checks are allowed.
    """
    daemon_threads = True

    def server_bind(self):
        socketserver.TCPServer.server_bind(self)
        self.server_name, self.server_port = self.server_address[:2]
