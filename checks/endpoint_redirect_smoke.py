#!/usr/bin/env python3
"""A model endpoint that redirects gets no second request and no key elsewhere.

batbox --print against an endpoint answering 307 fails with the status, the
Location and the base URL to set; the redirect target is never contacted. Both
the Anthropic wire and the OpenAI chat wire are checked.
"""
import argparse
import http.server
import json
import os
import subprocess
import tempfile
import threading
from pathlib import Path

from loopback_http import LoopbackHTTPServer

LIMITS = {'max_retries': 0}


class Recorder(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_POST(self):
        self.rfile.read(int(self.headers.get('Content-Length', '0')))
        with self.server.lock:
            self.server.requests.append(
                {'path': self.path, 'headers': {k.lower(): v for k, v in self.headers.items()}})
        if self.server.target:
            body = b'<html>moved</html>'
            self.send_response(307)
            self.send_header('Location', self.server.target + self.path)
            self.send_header('Content-Type', 'text/html')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        body = json.dumps({'type': 'error', 'error': {'type': 'api_error', 'message': 'target'}}).encode()
        self.send_response(500)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def serve(target):
    server = LoopbackHTTPServer(('127.0.0.1', 0), Recorder)
    server.lock = threading.Lock()
    server.requests = []
    server.target = target
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, thread


def run(binary, root, base, name, endpoint):
    config = root / f'{name}-config'
    config.mkdir()
    work = root / f'{name}-work'
    work.mkdir()
    endpoint = {'label': name, 'base_url': base, 'auth_header': 'x-api-key', 'auth_token': 'secret-key',
                'context_window': 200000, 'max_output_tokens': 4096, 'models': ['redirect-model'],
                **LIMITS, **endpoint}
    (config / 'native-client.json').write_text(json.dumps({
        'profile': f'{name}::redirect-model', 'endpoints': {name: endpoint},
        'state_directory': str(root / f'{name}-state'), 'mcpServers': {}, 'thinking': False,
        'auto_title': False}), encoding='utf-8')
    env = {key: value for key, value in os.environ.items() if not key.startswith('BATBOX_')}
    env.update(BATBOX_CONFIG_DIR=str(config), HOME=str(root),
               NO_PROXY='127.0.0.1,localhost', no_proxy='127.0.0.1,localhost')
    return subprocess.run([str(binary), '--cwd', str(work), '--no-thinking', '--print', 'Say ok'],
                          env=env, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=60)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    binary = parser.parse_args().binary.resolve(strict=True)
    target, target_thread = serve(None)
    moved = f'http://127.0.0.1:{target.server_port}'
    origin, origin_thread = serve(moved)
    base = f'http://127.0.0.1:{origin.server_port}'
    try:
        with tempfile.TemporaryDirectory(prefix='batbox-redirect-smoke-') as temporary:
            root = Path(temporary)
            for name, endpoint, path in (
                    ('anthropic', {}, '/v1/messages'),
                    ('chat', {'provider': 'openai-chat', 'auth_header': 'authorization',
                              'messages_path': '/v1/chat/completions'}, '/v1/chat/completions')):
                done = run(binary, root, base, name, endpoint)
                output = done.stdout + done.stderr
                assert done.returncode != 0, (name, done.returncode, output)
                assert f'307 redirect to {moved}{path}' in output, (name, output)
                assert f"Set this endpoint's base URL to {moved}" in output, (name, output)
                with origin.lock:
                    assert origin.requests, name
                    origin.requests.clear()
                with target.lock:
                    assert not target.requests, (name, target.requests)
    finally:
        for server, thread in ((origin, origin_thread), (target, target_thread)):
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
    print('a redirecting model endpoint fails with its Location and the key stays put')


if __name__ == '__main__':
    main()
