#!/usr/bin/env python3
"""batbox --print sends an endpoint's "betas" as anthropic-beta, and only there."""
import argparse
import http.server
import json
import os
import subprocess
import tempfile
import threading
from pathlib import Path

from loopback_http import LoopbackHTTPServer

BETAS = ['context-1m-2025-08-07', 'interleaved-thinking-2025-05-14']
REJECTED = 'bogus-beta-2099-01-01'
LIMITS = {'local': False, 'nonstreaming_fallback': False, 'request_timeout_ms': 60000,
          'stream_idle_timeout_ms': 900000, 'connect_timeout_ms': 3000,
          'nonstreaming_timeout_ms': 1800000, 'max_retries': 0}


def anthropic_events(model):
    return [
        ('message_start', {'type': 'message_start', 'message': {
            'type': 'message', 'id': 'msg-betas', 'model': model, 'role': 'assistant', 'content': [],
            'stop_reason': None, 'usage': {'input_tokens': 5, 'output_tokens': 0}}}),
        ('content_block_start', {'type': 'content_block_start', 'index': 0,
                                 'content_block': {'type': 'text', 'text': ''}}),
        ('content_block_delta', {'type': 'content_block_delta', 'index': 0,
                                 'delta': {'type': 'text_delta', 'text': 'betas ok'}}),
        ('content_block_stop', {'type': 'content_block_stop', 'index': 0}),
        ('message_delta', {'type': 'message_delta', 'delta': {'stop_reason': 'end_turn'},
                           'usage': {'output_tokens': 2}}),
        ('message_stop', {'type': 'message_stop'})]


def chat_chunks():
    def chunk(delta=None, finish=None, usage=None):
        body = {'id': 'chatcmpl-betas', 'object': 'chat.completion.chunk', 'model': 'served',
                'choices': [] if delta is None and finish is None else
                [{'index': 0, 'delta': delta or {}, 'finish_reason': finish}]}
        if usage is not None:
            body['usage'] = usage
        return body
    return [chunk({'role': 'assistant', 'content': 'betas ok'}), chunk(finish='stop'),
            chunk(usage={'prompt_tokens': 5, 'completion_tokens': 2, 'total_tokens': 7})]


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        headers = {key.lower(): value for key, value in self.headers.items()}
        with self.server.lock:
            self.server.requests.append({'path': self.path, 'body': body, 'headers': headers})
        if self.path.startswith('/capped/'):
            error = json.dumps({'type': 'error', 'error': {
                'type': 'api_error', 'message': 'Internal server error'}}).encode()
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(error)))
            self.end_headers()
            self.wfile.write(error)
            return
        if self.path.endswith('/chat/completions'):
            raw = b''.join(b'data: ' + json.dumps(item).encode() + b'\n\n' for item in chat_chunks())
            raw += b'data: [DONE]\n\n'
        elif REJECTED in headers.get('anthropic-beta', ''):
            error = json.dumps({'type': 'error', 'error': {
                'type': 'invalid_request_error',
                'message': f'Unexpected value(s) `{REJECTED}` for the `anthropic-beta` header. '
                           'Please consult our documentation or try again without the header.'}}).encode()
            self.send_response(400)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(error)))
            self.end_headers()
            self.wfile.write(error)
            return
        else:
            raw = b''.join(f'event: {event}\ndata: {json.dumps(value)}\n\n'.encode()
                           for event, value in anthropic_events(body.get('model', 'served')))
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Content-Length', str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)


def run(binary, root, base, name, endpoint):
    config = root / f'{name}-config'
    config.mkdir()
    work = root / f'{name}-work'
    work.mkdir()
    endpoint = {'label': name, 'base_url': base, 'auth_header': 'x-api-key', 'auth_token': 'betas-key',
                'context_window': 200000, 'max_output_tokens': 4096, 'models': ['betas-model'], **endpoint}
    (config / 'native-client.json').write_text(json.dumps({
        'profile': f'{name}::betas-model', 'endpoints': {name: endpoint},
        'state_directory': str(root / f'{name}-state'), 'mcpServers': {}, 'thinking': False,
        'auto_title': False}), encoding='utf-8')
    env = {key: value for key, value in os.environ.items() if not key.startswith('BATBOX_')}
    env.update(BATBOX_CONFIG_DIR=str(config), HOME=str(root),
               NO_PROXY='127.0.0.1,localhost', no_proxy='127.0.0.1,localhost')
    return subprocess.run([str(binary), '--cwd', str(work), '--no-thinking', '--print', 'Say ok'],
                          env=env, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=60)


def single(server, name):
    with server.lock:
        calls = list(server.requests)
        server.requests.clear()
    if not calls:
        raise AssertionError(f'{name}: no request reached the server')
    return calls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    binary = parser.parse_args().binary.resolve(strict=True)
    server = LoopbackHTTPServer(('127.0.0.1', 0), Handler)
    server.lock = threading.Lock()
    server.requests = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f'http://127.0.0.1:{server.server_port}'
    try:
        with tempfile.TemporaryDirectory(prefix='batbox-betas-smoke-') as temporary:
            root = Path(temporary)

            version = subprocess.run([str(binary), 'version'], capture_output=True, text=True,
                                     timeout=30).stdout.split('\n')[0].split(' ')[1]
            done = run(binary, root, base, 'plain', {})
            assert done.returncode == 0 and 'betas ok' in done.stdout, (done.returncode, done.stdout, done.stderr)
            for call in single(server, 'plain'):
                assert call['headers'].get('user-agent') == f'batbox/{version}', call['headers']
                assert 'anthropic-beta' not in call['headers'], call['headers']
                assert 'batbox-beta' not in call['headers'], call['headers']
                assert 'betas' not in call['body'], sorted(call['body'])

            done = run(binary, root, base, 'anthropic', {'betas': BETAS})
            assert done.returncode == 0 and 'betas ok' in done.stdout, (done.returncode, done.stdout, done.stderr)
            for call in single(server, 'anthropic'):
                assert call['headers'].get('anthropic-beta') == ','.join(BETAS), call['headers']
                assert 'batbox-beta' not in call['headers'], call['headers']
                assert 'betas' not in call['body'], sorted(call['body'])

            done = run(binary, root, base, 'chat', {'provider': 'openai-chat', 'auth_header': 'authorization',
                                                    'messages_path': '/v1/chat/completions', 'betas': BETAS})
            assert done.returncode == 0 and 'betas ok' in done.stdout, (done.returncode, done.stdout, done.stderr)
            for call in single(server, 'chat'):
                assert call['path'].endswith('/chat/completions'), call['path']
                assert 'anthropic-beta' not in call['headers'], call['headers']
                assert 'betas' not in call['body'], sorted(call['body'])

            done = run(binary, root, base, 'limited', {'betas': BETAS, **LIMITS})
            assert done.returncode == 0 and 'betas ok' in done.stdout, (done.returncode, done.stdout, done.stderr)
            for call in single(server, 'limited'):
                assert call['headers'].get('anthropic-beta') == ','.join(BETAS), call['headers']
                assert 'betas' not in call['body'], sorted(call['body'])

            done = run(binary, root, f'{base}/capped', 'capped', {'betas': BETAS, **LIMITS})
            assert done.returncode != 0, (done.returncode, done.stdout, done.stderr)
            calls = single(server, 'capped')
            assert len(calls) == 1, [call['path'] for call in calls]
            assert calls[0]['headers'].get('anthropic-beta') == ','.join(BETAS), calls[0]['headers']

            done = run(binary, root, base, 'rejected', {'betas': [BETAS[0], REJECTED]})
            output = done.stdout + done.stderr
            assert done.returncode != 0, (done.returncode, output)
            assert (f'does not accept anthropic-beta {REJECTED} · Remove it from this endpoint\'s "betas" setting'
                    in output), output
            single(server, 'rejected')
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
    print('endpoint betas reach anthropic-beta on anthropic endpoints only, alongside request limits')


if __name__ == '__main__':
    main()
