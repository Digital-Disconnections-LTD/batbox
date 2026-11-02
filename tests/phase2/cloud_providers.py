#!/usr/bin/env python3
"""End-to-end Bedrock and Vertex AI client checks against local fake servers."""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cloud_fakes import FakeCloud, answer_events, tool_use_events  # noqa: E402

SCRUB = ('AWS_', 'GOOGLE_', 'GCE_', 'CLOUDSDK_', 'GCLOUD_', 'GCP_', 'VERTEX_')


class Runner:
    def __init__(self, probe, home):
        self.probe = probe
        self.home = home
        self.counter = 0

    def env(self, extra=None, path=None):
        env = {k: v for k, v in os.environ.items() if not k.startswith(SCRUB)}
        env['HOME'] = self.home
        env['PATH'] = path if path is not None else '/usr/bin:/bin'
        env.update(extra or {})
        return env

    def write(self, name, payload):
        self.counter += 1
        path = os.path.join(self.home, f'{self.counter}-{name}')
        with open(path, 'w') as handle:
            handle.write(payload if isinstance(payload, str) else json.dumps(payload))
        return path

    def run(self, kind, action, config, body=None, repeat=1, env=None, path=None):
        args = [self.probe, kind, action, self.write('config.json', config)]
        if body is not None or repeat != 1:
            args.append(self.write('body.json', body or {}))
        if repeat != 1:
            args.append(str(repeat))
        process = subprocess.run(args, capture_output=True, text=True, timeout=60, env=self.env(env, path))
        if process.returncode:
            raise AssertionError(process.stderr)
        return json.loads(process.stdout)['results']


def first_turn():
    return {'model': 'us.anthropic.claude-opus-5-5-v1:0', 'max_tokens': 256, 'stream': True,
            'system': [{'type': 'text', 'text': 'You are terse.'}],
            'tools': [{'name': 'read', 'description': 'Read a file', 'input_schema': {'type': 'object', 'properties': {'path': {'type': 'string'}}}}],
            'messages': [{'role': 'user', 'content': 'What does a.txt say?'}]}


def second_turn():
    body = first_turn()
    body['messages'] += [
        {'role': 'assistant', 'content': [{'type': 'text', 'text': 'Reading the file.'},
                                          {'type': 'tool_use', 'id': 'toolu_01', 'name': 'read', 'input': {'path': 'a.txt'}}]},
        {'role': 'user', 'content': [{'type': 'tool_result', 'tool_use_id': 'toolu_01', 'content': 'hello'}]}]
    return body


def expect_clean(server):
    assert not server.failures, server.failures


def bedrock_checks(runner, server):
    base = {'region': 'us-west-2', 'base_url': server.url}
    model = 'us.anthropic.claude-opus-5-5-v1:0'

    server.reset()
    server.secrets = {'AKIAENVKEY': 'envsecret'}
    env = {'AWS_ACCESS_KEY_ID': 'AKIAENVKEY', 'AWS_SECRET_ACCESS_KEY': 'envsecret', 'AWS_SESSION_TOKEN': 'envsession'}
    first, second = runner.run('bedrock', 'stream', base, first_turn(), env=env)[0], runner.run('bedrock', 'stream', base, second_turn(), env=env)[0]
    expect_clean(server)
    assert first['status'] == 200 and first['headers_reported'] and first['request_id'] == 'req-stream', first
    assert first['events'] == tool_use_events(model), first['events']
    assert second['events'] == answer_events(model), second['events']
    calls = [r for r in server.requests if r['kind'] == 'bedrock']
    assert [c['model'] for c in calls] == [model, model] and all(c['stream'] for c in calls)
    assert all(c['token'] == 'envsession' and c['accept'] == 'application/vnd.amazon.eventstream' for c in calls), calls
    assert calls[0]['body']['tools'][0]['name'] == 'read' and calls[0]['body']['system'][0]['text'] == 'You are terse.'

    invoke = runner.run('bedrock', 'invoke', base, second_turn(), env=env)[0]
    expect_clean(server)
    assert invoke['status'] == 200 and invoke['body']['content'][0]['text'] == 'The file says hello.', invoke
    assert invoke['request_id'] == 'req-invoke' and invoke['events'] == []

    server.reset()
    server.bearer = 'bedrock-api-key'
    keyed = runner.run('bedrock', 'stream', base, first_turn(), env={'AWS_BEARER_TOKEN_BEDROCK': 'bedrock-api-key'})[0]
    expect_clean(server)
    assert keyed['events'] == tool_use_events(model), keyed
    configured = runner.run('bedrock', 'stream', {**base, 'bearer_token': 'bedrock-api-key'}, first_turn())[0]
    expect_clean(server)
    assert configured['status'] == 200

    server.reset()
    server.secrets = {'AKIAENVKEY': 'envsecret'}
    server.scenario = 'throttle'
    throttled = runner.run('bedrock', 'stream', base, first_turn(), env=env)[0]
    error = throttled['error']
    assert error['status'] == 429 and error['body']['error']['type'] == 'rate_limit_error', error
    assert 'rate_limit_error' in error['message'] and throttled['events'] == []

    server.reset()
    server.secrets = {'AKIAENVKEY': 'envsecret'}
    server.scenario = 'midstream-exception'
    broken = runner.run('bedrock', 'stream', base, first_turn(), env=env)[0]
    assert len(broken['events']) == 3 and broken['error']['status'] == 500, broken
    assert 'modelStreamErrorException' in broken['error']['body']['error']['message']

    # credential_process output is stashed, refreshed after an expired-token reply, and the request retried once.
    server.reset()
    server.secrets = {'AKIAPROCESS': 'processsecret'}
    server.scenario = 'expire-once'
    aws_dir = os.path.join(runner.home, '.aws')
    os.makedirs(aws_dir, exist_ok=True)
    counter = os.path.join(runner.home, 'process-runs')
    script = runner.write('process.sh', '#!/bin/sh\necho run >> "%s"\necho \'{"Version":1,"AccessKeyId":"AKIAPROCESS",'
                                         '"SecretAccessKey":"processsecret","SessionToken":"proc","Expiration":"2099-01-01T00:00:00Z"}\'\n' % counter)
    os.chmod(script, 0o700)
    with open(os.path.join(aws_dir, 'config'), 'w') as handle:
        handle.write(f'[profile builder]\nregion = us-west-2\ncredential_process = {script}\n')
    results = runner.run('bedrock', 'stream', {'base_url': server.url}, first_turn(), repeat=2, env={'AWS_PROFILE': 'builder'})
    expect_clean(server)
    assert all(r['status'] == 200 and r['events'] == tool_use_events(model) for r in results), results
    assert server.count('bedrock') == 3, server.requests
    with open(counter) as handle:
        runs = handle.read().count('run')
    assert runs == 2, f'credential_process ran {runs} times; expected once plus one refresh'

    # The application builds a client per request; the factory's shared stash keeps the process from rerunning.
    server.reset()
    server.secrets = {'AKIAPROCESS': 'processsecret'}
    os.remove(counter)
    results = runner.run('factory-bedrock', 'stream', {'base_url': server.url}, first_turn(), repeat=3, env={'AWS_PROFILE': 'builder'})
    expect_clean(server)
    assert all(r['status'] == 200 for r in results), results
    with open(counter) as handle:
        runs = handle.read().count('run')
    assert runs == 1, f'credential_process ran {runs} times across fresh clients'

    # role_arn + source_profile: STS AssumeRole signed with the source keys, Bedrock signed with the role keys.
    server.reset()
    server.sts_secrets = {'AKIASOURCE': 'sourcesecret'}
    with open(os.path.join(aws_dir, 'credentials'), 'w') as handle:
        handle.write('[source]\naws_access_key_id = AKIASOURCE\naws_secret_access_key = sourcesecret\n')
    with open(os.path.join(aws_dir, 'config'), 'w') as handle:
        handle.write('[profile deploy]\nrole_arn = arn:aws:iam::123456789012:role/bedrock\nsource_profile = source\n'
                     'role_session_name = batbox-test\nregion = eu-west-3\n')
    assumed = runner.run('bedrock', 'stream', {'base_url': server.url}, first_turn(), repeat=2,
                         env={'AWS_PROFILE': 'deploy', 'AWS_ENDPOINT_URL_STS': server.url})
    expect_clean(server)
    assert all(r['status'] == 200 for r in assumed), assumed
    sts = [r for r in server.requests if r['kind'] == 'sts']
    assert len(sts) == 1 and sts[0]['form']['RoleArn'] == 'arn:aws:iam::123456789012:role/bedrock', sts
    assert sts[0]['form']['RoleSessionName'] == 'batbox-test' and sts[0]['who'] == 'AKIASOURCE'
    assert all(r['who'] == 'ASIAASSUMED' and r['token'] == 'assumedtoken' for r in server.requests if r['kind'] == 'bedrock')
    os.remove(os.path.join(aws_dir, 'credentials'))
    os.remove(os.path.join(aws_dir, 'config'))

    server.reset()
    server.secrets = {'AKIAENVKEY': 'envsecret'}
    listed = runner.run('bedrock', 'models', {'region': 'us-west-2'}, env={**env, 'AWS_ENDPOINT_URL_BEDROCK': server.url})[0]
    expect_clean(server)
    ids = [m['id'] for m in listed['models']['data']]
    assert ids == ['us.anthropic.claude-opus-5-5-v1:0', 'global.anthropic.claude-sonnet-5-5-v1:0',
                   'anthropic.claude-haiku-4-5-20251001-v1:0'], ids
    pages = [r['query'] for r in server.requests if r['kind'] == 'inference-profiles']
    assert len(pages) == 2 and pages[1].get('nextToken') == 'page/2' and pages[0].get('typeEquals') == 'SYSTEM_DEFINED', pages

    server.reset()
    missing = runner.run('bedrock', 'stream', base, first_turn())[0]
    assert missing['error']['status'] == 401 and 'AWS_BEARER_TOKEN_BEDROCK' in missing['error']['message'], missing
    assert server.requests == []


def vertex_checks(runner, server):
    model = 'claude-opus-5-5'
    body = {**first_turn(), 'model': model}
    answer = {**second_turn(), 'model': model}
    base = {'project': 'my-project', 'region': 'us-east5', 'base_url': server.url, 'gcloud_command': ''}

    server.reset()
    key_path = os.path.join(runner.home, 'sa-key.pem')
    process = subprocess.run([runner.probe, 'genkey', key_path], capture_output=True, text=True, timeout=60, check=True)
    numbers = json.loads(process.stdout)
    server.rsa = (int(numbers['n'], 16), int(numbers['e'], 16))
    with open(key_path) as handle:
        private_key = handle.read()
    account = runner.write('service-account.json', {
        'type': 'service_account', 'project_id': 'sa-project', 'private_key_id': 'key-1', 'private_key': private_key,
        'client_email': 'bot@project.iam.gserviceaccount.com', 'token_uri': f'{server.url}/token'})
    os.remove(key_path)
    results = runner.run('vertex', 'stream', base, body, repeat=2, env={'GOOGLE_APPLICATION_CREDENTIALS': account})
    expect_clean(server)

    assert all(r['status'] == 200 and r['headers_reported'] for r in results), results
    assert results[0]['events'] == tool_use_events(model), results[0]['events']
    assert server.count('token') == 1, 'the access token is stashed between requests'
    fresh = runner.run('factory-vertex', 'stream', {k: v for k, v in base.items() if k != 'gcloud_command'}, body, repeat=3,
                       env={'GOOGLE_APPLICATION_CREDENTIALS': account})
    expect_clean(server)
    assert all(r['status'] == 200 for r in fresh) and server.count('token') == 2, server.requests
    calls = [r for r in server.requests if r['kind'] == 'vertex']
    assert all(c['project'] == 'my-project' and c['region'] == 'us-east5' and c['model'] == model and c['stream'] for c in calls), calls
    assert calls[0]['auth'] == 'Bearer ya29.fake-1'
    second = runner.run('vertex', 'stream', base, answer, env={'GOOGLE_APPLICATION_CREDENTIALS': account})[0]
    assert second['events'] == answer_events(model), second
    invoke = runner.run('vertex', 'invoke', base, answer, env={'GOOGLE_APPLICATION_CREDENTIALS': account})[0]
    expect_clean(server)
    assert invoke['body']['content'][0]['text'] == 'The file says hello.', invoke

    # The project falls back to the service-account file and the region to global.
    server.tokens_issued.clear()
    fallback = runner.run('vertex', 'stream', {'base_url': server.url, 'gcloud_command': ''}, body, env={'GOOGLE_APPLICATION_CREDENTIALS': account})[0]
    expect_clean(server)
    last = [r for r in server.requests if r['kind'] == 'vertex'][-1]
    assert fallback['status'] == 200 and last['project'] == 'sa-project' and last['region'] == 'global', last

    server.scenario = 'unauth-once'
    server.requests.clear()
    retried = runner.run('vertex', 'stream', base, body, env={'GOOGLE_APPLICATION_CREDENTIALS': account})[0]
    expect_clean(server)
    assert retried['status'] == 200 and server.count('token') == 2 and server.count('vertex') == 2, server.requests

    server.scenario = 'quota'
    quota = runner.run('vertex', 'stream', base, body, env={'GOOGLE_APPLICATION_CREDENTIALS': account})[0]
    assert quota['error']['status'] == 429 and quota['error']['body']['error']['type'] == 'rate_limit_error', quota

    server.scenario = 'overloaded-midstream'
    overloaded = runner.run('vertex', 'stream', base, body, env={'GOOGLE_APPLICATION_CREDENTIALS': account})[0]
    assert len(overloaded['events']) == 2 and overloaded['error']['body']['error']['type'] == 'overloaded_error', overloaded

    # gcloud application-default login file (authorized_user refresh token).
    server.reset()
    server.refresh_token = 'refresh-1'
    adc_dir = os.path.join(runner.home, '.config', 'gcloud')
    os.makedirs(adc_dir, exist_ok=True)
    with open(os.path.join(adc_dir, 'application_default_credentials.json'), 'w') as handle:
        json.dump({'type': 'authorized_user', 'client_id': 'client-1', 'client_secret': 'secret-1', 'refresh_token': 'refresh-1',
                   'token_uri': f'{server.url}/token'}, handle)
    user = runner.run('vertex', 'stream', base, body)[0]
    expect_clean(server)
    assert user['status'] == 200 and server.requests[0]['grant'] == 'refresh_token', server.requests
    shutil.rmtree(os.path.join(runner.home, '.config'))

    # gcloud auth print-access-token, found on PATH.
    server.reset()
    bin_dir = os.path.join(runner.home, 'bin')
    os.makedirs(bin_dir, exist_ok=True)
    gcloud = os.path.join(bin_dir, 'gcloud')
    with open(gcloud, 'w') as handle:
        handle.write('#!/bin/sh\n[ "$1 $2" = "auth print-access-token" ] || exit 2\necho ya29.from-gcloud\n')
    os.chmod(gcloud, 0o700)
    server.tokens_issued.append('ya29.from-gcloud')
    cli = runner.run('vertex', 'stream', {k: v for k, v in base.items() if k != 'gcloud_command'}, body, path=f'{bin_dir}:/usr/bin:/bin')[0]
    expect_clean(server)
    assert cli['status'] == 200 and server.requests[-1]['auth'] == 'Bearer ya29.from-gcloud', server.requests

    # Metadata server when nothing else is available.
    server.reset()
    server.metadata_token = 'ya29.metadata'
    host = server.url[len('http://'):]
    meta = runner.run('vertex', 'stream', {k: v for k, v in base.items() if k != 'gcloud_command'}, body, env={'GCE_METADATA_HOST': host})[0]
    expect_clean(server)
    assert meta['status'] == 200 and server.requests[0] == {'kind': 'metadata', 'flavor': 'Google'}, server.requests

    # A configured token skips ADC entirely.
    server.reset()
    server.bearer = 'ya29.configured'
    configured = runner.run('vertex', 'stream', {**base, 'bearer_token': 'ya29.configured'}, body)[0]
    expect_clean(server)
    assert configured['status'] == 200 and server.count('token') == 0

    server.reset()
    none = runner.run('vertex', 'stream', base, body, env={'GCE_METADATA_HOST': '127.0.0.1:9'})[0]
    assert none['error']['status'] == 401 and 'GOOGLE_APPLICATION_CREDENTIALS' in none['error']['message'], none

    models = runner.run('vertex', 'models', base)[0]['models']['data']
    assert models[0]['id'] == 'claude-opus-5-5' and all(m['id'].startswith('claude-') for m in models), models


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    probe = parser.parse_args().probe
    server = FakeCloud()
    home = tempfile.mkdtemp(prefix='batbox-cloud-')
    try:
        runner = Runner(probe, home)
        bedrock_checks(runner, server)
        vertex_checks(runner, server)
    finally:
        server.shutdown()
        shutil.rmtree(home, ignore_errors=True)
    print('Bedrock SigV4/bearer/process/assume-role/event-stream/models and Vertex service-account/ADC/gcloud/metadata/SSE checks passed')


if __name__ == '__main__':
    main()
