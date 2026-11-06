#!/usr/bin/env python3
"""Run batbox --print through Bedrock and Vertex AI endpoints served by local fakes, including a Read gadget round trip."""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tests' / 'phase2'))
from cloud_fakes import FakeCloud  # noqa: E402

MARKER = 'batbox-cloud-marker-4471'
BETAS = ['context-1m-2025-08-07', 'interleaved-thinking-2025-05-14']


def responder(note):
    def turn(body, model):
        results = [block for message in body.get('messages', []) if isinstance(message.get('content'), list)
                   for block in message['content'] if isinstance(block, dict) and block.get('type') == 'tool_result']
        start = {'type': 'message_start', 'message': {'id': f'msg-{len(results)}', 'type': 'message', 'role': 'assistant',
                                                      'model': model, 'content': [], 'stop_reason': None,
                                                      'usage': {'input_tokens': 30, 'output_tokens': 1}}}
        if not results:
            arguments = json.dumps({'file_path': str(note)})
            return [start,
                    {'type': 'content_block_start', 'index': 0, 'content_block': {'type': 'tool_use', 'id': 'toolu_cloud_1', 'name': 'Read', 'input': {}}},
                    {'type': 'content_block_delta', 'index': 0, 'delta': {'type': 'input_json_delta', 'partial_json': arguments[:10]}},
                    {'type': 'content_block_delta', 'index': 0, 'delta': {'type': 'input_json_delta', 'partial_json': arguments[10:]}},
                    {'type': 'content_block_stop', 'index': 0},
                    {'type': 'message_delta', 'delta': {'stop_reason': 'tool_use'}, 'usage': {'output_tokens': 12}},
                    {'type': 'message_stop'}]
        echoed = MARKER if MARKER in json.dumps(results) else 'tool result missing'
        return [start,
                {'type': 'content_block_start', 'index': 0, 'content_block': {'type': 'text', 'text': ''}},
                {'type': 'content_block_delta', 'index': 0, 'delta': {'type': 'text_delta', 'text': f'The note says {echoed}.'}},
                {'type': 'content_block_stop', 'index': 0},
                {'type': 'message_delta', 'delta': {'stop_reason': 'end_turn'}, 'usage': {'output_tokens': 8}},
                {'type': 'message_stop'}]
    return turn


def run_case(binary, server, root, name, endpoint, model, extra_env):
    work = root / f'{name}-work'
    work.mkdir()
    note = work / 'note.txt'
    note.write_text(f'{MARKER}\n', encoding='utf-8')
    config_dir = root / f'{name}-config'
    config_dir.mkdir()
    config = {'profile': f'{name}::{model}', 'endpoints': {name: endpoint}, 'state_directory': str(root / f'{name}-state'),
              'mcpServers': {}, 'thinking': False, 'auto_title': False, 'compact_at': 60000}
    (config_dir / 'native-client.json').write_text(json.dumps(config), encoding='utf-8')
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('BATBOX_', 'AWS_', 'GOOGLE_', 'GCE_', 'CLOUDSDK_'))}
    env.update(BATBOX_CONFIG_DIR=str(config_dir), HOME=str(root), NO_PROXY='127.0.0.1,localhost', no_proxy='127.0.0.1,localhost')
    env.update(extra_env)
    server.responder = responder(note)
    process = subprocess.run([str(binary), '--cwd', str(work), '--no-thinking', '--print', 'What does note.txt say?'],
                             env=env, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=60)
    if process.returncode != 0 or f'The note says {MARKER}.' not in process.stdout:
        raise AssertionError(f'{name}: exit {process.returncode}\nstdout: {process.stdout[-2000:]}\nstderr: {process.stderr[-2000:]}')
    if server.failures:
        raise AssertionError(f'{name}: {server.failures}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    binary = parser.parse_args().binary
    server = FakeCloud()
    try:
        with tempfile.TemporaryDirectory(prefix='batbox-cloud-smoke-') as temporary:
            root = Path(temporary)
            server.reset()
            server.secrets = {'AKIASMOKE': 'smokesecret'}
            bedrock_model = 'us.anthropic.claude-opus-5-5-v1:0'
            run_case(binary, server, root, 'bedrock', {
                'label': 'Bedrock', 'provider': 'bedrock', 'region': 'us-west-2', 'base_url': server.url,
                'models': [bedrock_model], 'context_window': 200000, 'max_output_tokens': 4096, 'betas': BETAS},
                bedrock_model, {'AWS_ACCESS_KEY_ID': 'AKIASMOKE', 'AWS_SECRET_ACCESS_KEY': 'smokesecret'})
            calls = [r for r in server.requests if r['kind'] == 'bedrock']
            assert len(calls) == 2 and all(c['who'] == 'AKIASMOKE' and c['stream'] and c['model'] == bedrock_model for c in calls), calls
            assert all(c['body'].get('anthropic_beta') == BETAS and 'betas' not in c['body'] for c in calls), calls

            server.reset()
            server.bearer = 'ya29.smoke'
            run_case(binary, server, root, 'vertex', {
                'label': 'Vertex', 'provider': 'vertex', 'project': 'smoke-project', 'region': 'us-east5', 'base_url': server.url,
                'auth_token': 'ya29.smoke', 'models': ['claude-opus-5-5'], 'context_window': 200000, 'max_output_tokens': 4096,
                'betas': BETAS}, 'claude-opus-5-5', {})
            calls = [r for r in server.requests if r['kind'] == 'vertex']
            assert len(calls) == 2 and all(c['project'] == 'smoke-project' and c['region'] == 'us-east5' and c['stream'] for c in calls), calls
            assert all(c['auth'] == 'Bearer ya29.smoke' for c in calls), calls
            assert all(c['beta'] == ','.join(BETAS) and 'betas' not in c['body'] for c in calls), calls
    finally:
        server.shutdown()
    print('batbox --print Bedrock and Vertex AI Read round trips passed')


if __name__ == '__main__':
    main()
