"""Fake Bedrock, STS, Vertex AI and Google token servers for the cloud provider checks."""
import base64
import hashlib
import hmac
import json
import struct
import threading
import urllib.parse
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# ---- AWS event-stream encoding -------------------------------------------------

def es_header(name, kind, value=None):
    raw = name.encode()
    out = bytes([len(raw)]) + raw + bytes([kind])
    if kind in (0, 1):
        return out
    if kind == 2:
        return out + struct.pack('>b', value)
    if kind == 3:
        return out + struct.pack('>h', value)
    if kind == 4:
        return out + struct.pack('>i', value)
    if kind in (5, 8):
        return out + struct.pack('>q', value)
    if kind in (6, 7):
        data = value if isinstance(value, bytes) else value.encode()
        return out + struct.pack('>H', len(data)) + data
    if kind == 9:
        return out + value
    raise ValueError(kind)


def es_message(headers, payload):
    head = b''.join(headers)
    total = 16 + len(head) + len(payload)
    prelude = struct.pack('>II', total, len(head))
    prelude += struct.pack('>I', zlib.crc32(prelude))
    body = prelude + head + payload
    return body + struct.pack('>I', zlib.crc32(body))


def es_chunk(event):
    data = base64.b64encode(json.dumps(event).encode()).decode()
    payload = json.dumps({'bytes': data, 'p': 'abcdefghij'}).encode()
    return es_message([es_header(':event-type', 7, 'chunk'), es_header(':content-type', 7, 'application/json'),
                       es_header(':message-type', 7, 'event')], payload)


def es_exception(name, message):
    return es_message([es_header(':exception-type', 7, name), es_header(':content-type', 7, 'application/json'),
                       es_header(':message-type', 7, 'exception')], json.dumps({'message': message}).encode())


def es_error(code, message):
    return es_message([es_header(':error-code', 7, code), es_header(':error-message', 7, message),
                       es_header(':message-type', 7, 'error')], b'')

# ---- Anthropic conversations ----------------------------------------------------

def tool_use_events(model='claude-test'):
    return [
        {'type': 'message_start', 'message': {'id': 'msg_1', 'type': 'message', 'role': 'assistant', 'model': model, 'content': [],
                                              'stop_reason': None, 'stop_sequence': None,
                                              'usage': {'input_tokens': 12, 'output_tokens': 1}}},
        {'type': 'content_block_start', 'index': 0, 'content_block': {'type': 'text', 'text': ''}},
        {'type': 'content_block_delta', 'index': 0, 'delta': {'type': 'text_delta', 'text': 'Reading the file.'}},
        {'type': 'content_block_stop', 'index': 0},
        {'type': 'content_block_start', 'index': 1, 'content_block': {'type': 'tool_use', 'id': 'toolu_01', 'name': 'read', 'input': {}}},
        {'type': 'content_block_delta', 'index': 1, 'delta': {'type': 'input_json_delta', 'partial_json': '{"path":'}},
        {'type': 'content_block_delta', 'index': 1, 'delta': {'type': 'input_json_delta', 'partial_json': '"a.txt"}'}},
        {'type': 'content_block_stop', 'index': 1},
        {'type': 'message_delta', 'delta': {'stop_reason': 'tool_use', 'stop_sequence': None}, 'usage': {'output_tokens': 30}},
        {'type': 'message_stop'},
    ]


def answer_events(model='claude-test'):
    return [
        {'type': 'message_start', 'message': {'id': 'msg_2', 'type': 'message', 'role': 'assistant', 'model': model, 'content': [],
                                              'stop_reason': None, 'stop_sequence': None,
                                              'usage': {'input_tokens': 40, 'output_tokens': 1}}},
        {'type': 'content_block_start', 'index': 0, 'content_block': {'type': 'text', 'text': ''}},
        {'type': 'content_block_delta', 'index': 0, 'delta': {'type': 'text_delta', 'text': 'The file says hello.'}},
        {'type': 'content_block_stop', 'index': 0},
        {'type': 'message_delta', 'delta': {'stop_reason': 'end_turn', 'stop_sequence': None}, 'usage': {'output_tokens': 6}},
        {'type': 'message_stop'},
    ]


def wants_answer(body):
    messages = body.get('messages') or []
    if not messages:
        return False
    content = messages[-1].get('content')
    return isinstance(content, list) and any(isinstance(b, dict) and b.get('type') == 'tool_result' for b in content)


def message_json(events):
    start = events[0]['message']
    return {**start, 'content': [{'type': 'text', 'text': 'The file says hello.'}], 'stop_reason': 'end_turn',
            'usage': {'input_tokens': 40, 'output_tokens': 6}}

# ---- SigV4 verification (independent of the C++ signer) --------------------------

def _canonical_path(path):
    return urllib.parse.quote(path, safe='/~')


def _canonical_query(query):
    pairs = urllib.parse.parse_qsl(query, keep_blank_values=True)
    encoded = sorted((urllib.parse.quote(k, safe='-_.~'), urllib.parse.quote(v, safe='-_.~')) for k, v in pairs)
    return '&'.join(f'{k}={v}' for k, v in encoded)


def verify_sigv4(method, raw_path, headers, body, secrets, service):
    """Returns (access_key_id, None) when the Authorization header is valid, else (None, reason)."""
    auth = headers.get('Authorization', '')
    if not auth.startswith('AWS4-HMAC-SHA256 '):
        return None, 'no SigV4 authorization header'
    fields = dict(part.strip().split('=', 1) for part in auth[len('AWS4-HMAC-SHA256 '):].split(','))
    akid, date, region, svc, terminal = fields['Credential'].split('/')
    if svc != service or terminal != 'aws4_request':
        return None, f'bad scope {fields["Credential"]}'
    if akid not in secrets:
        return None, f'unknown access key {akid}'
    signed = fields['SignedHeaders'].split(';')
    for required in ('host', 'x-amz-date'):
        if required not in signed:
            return None, f'{required} is not signed'
    if hashlib.sha256(body).hexdigest() != headers.get('x-amz-content-sha256', hashlib.sha256(body).hexdigest()):
        return None, 'x-amz-content-sha256 does not match the body'
    path, _, query = raw_path.partition('?')
    canonical_headers = ''
    for name in signed:
        values = headers.get_all(name) or []
        canonical_headers += name + ':' + ','.join(' '.join(v.split()) for v in values) + '\n'
    canonical = '\n'.join([method, _canonical_path(path), _canonical_query(query), canonical_headers, ';'.join(signed),
                           hashlib.sha256(body).hexdigest()])
    amz_date = headers['x-amz-date']
    scope = f'{date}/{region}/{svc}/aws4_request'
    to_sign = '\n'.join(['AWS4-HMAC-SHA256', amz_date, scope, hashlib.sha256(canonical.encode()).hexdigest()])
    key = ('AWS4' + secrets[akid]).encode()
    for part in (date, region, svc, 'aws4_request'):
        key = hmac.new(key, part.encode(), hashlib.sha256).digest()
    expected = hmac.new(key, to_sign.encode(), hashlib.sha256).hexdigest()
    if expected != fields['Signature']:
        return None, 'signature mismatch'
    return akid, None

# ---- RS256 verification with Python integers ---------------------------------------

_SHA256_PREFIX = bytes.fromhex('3031300d060960864801650304020105000420')


def b64url(text):
    return base64.urlsafe_b64decode(text + '=' * (-len(text) % 4))


def verify_rs256(jwt, n, e):
    head, claims, signature = jwt.split('.')
    size = (n.bit_length() + 7) // 8
    decoded = pow(int.from_bytes(b64url(signature), 'big'), e, n).to_bytes(size, 'big')
    digest = hashlib.sha256(f'{head}.{claims}'.encode()).digest()
    expected = b'\x00\x01' + b'\xff' * (size - 3 - len(_SHA256_PREFIX) - len(digest)) + b'\x00' + _SHA256_PREFIX + digest
    if decoded != expected:
        return None
    return json.loads(b64url(head)), json.loads(b64url(claims))

# ---- Server ---------------------------------------------------------------------

class FakeCloud(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(('127.0.0.1', 0), Handler)
        self.lock = threading.Lock()
        self.reset()
        self.thread = threading.Thread(target=self.serve_forever, daemon=True)
        self.thread.start()

    @property
    def url(self):
        return f'http://127.0.0.1:{self.server_address[1]}'

    def reset(self):
        self.secrets = {}
        self.sts_secrets = {}
        self.bearer = None
        self.scenario = None
        self.requests = []
        self.failures = []
        self.rsa = None
        self.tokens_issued = []
        self.refresh_token = None
        self.metadata_token = None
        # Optional callable(body, model) -> Anthropic events, replacing the canned conversation.
        self.responder = None

    def record(self, entry):
        with self.lock:
            self.requests.append(entry)

    def fail(self, reason):
        with self.lock:
            self.failures.append(reason)

    def count(self, kind):
        return sum(1 for r in self.requests if r['kind'] == kind)


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def body(self):
        return self.rfile.read(int(self.headers.get('Content-Length', '0')))

    def reply(self, status, payload, content_type='application/json', headers=None):
        data = payload if isinstance(payload, bytes) else json.dumps(payload).encode()
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        for name, value in (headers or {}).items():
            self.send_header(name, value)
        self.end_headers()
        self.wfile.write(data)

    def stream(self, content_type, frames, headers=None):
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Connection', 'close')
        for name, value in (headers or {}).items():
            self.send_header(name, value)
        self.end_headers()
        for frame in frames:
            self.wfile.write(frame)
            self.wfile.flush()
        self.close_connection = True

    def conversation(self, body, model):
        if self.server.responder:
            return self.server.responder(body, model)
        return answer_events(model) if wants_answer(body) else tool_use_events(model)

    def aws_auth(self, raw, service='bedrock'):
        server = self.server
        if server.bearer is not None:
            if self.headers.get('Authorization') != f'Bearer {server.bearer}':
                return None, 'missing Bedrock API key'
            if self.headers.get('x-amz-date'):
                return None, 'bearer requests must not be signed'
            return 'bearer', None
        return verify_sigv4(self.command, self.path, self.headers, raw, server.secrets if service == 'bedrock' else server.sts_secrets, service)

    def do_GET(self):
        server = self.server
        parsed = urllib.parse.urlsplit(self.path)
        query = dict(urllib.parse.parse_qsl(parsed.query))
        if parsed.path == '/computeMetadata/v1/instance/service-accounts/default/token':
            server.record({'kind': 'metadata', 'flavor': self.headers.get('Metadata-Flavor')})
            if self.headers.get('Metadata-Flavor') != 'Google' or not server.metadata_token:
                return self.reply(404, {'error': 'not here'})
            return self.reply(200, {'access_token': server.metadata_token, 'expires_in': 3599, 'token_type': 'Bearer'})
        who, reason = self.aws_auth(b'')
        if reason:
            server.fail(f'GET {self.path}: {reason}')
            return self.reply(403, {'message': reason}, headers={'x-amzn-ErrorType': 'InvalidSignatureException'})
        if parsed.path == '/foundation-models':
            server.record({'kind': 'foundation-models', 'query': query, 'who': who})
            if query.get('byProvider') != 'anthropic':
                server.fail('foundation-models without byProvider=anthropic')
            return self.reply(200, {'modelSummaries': [
                {'modelId': 'anthropic.claude-haiku-4-5-20251001-v1:0', 'modelName': 'Claude Haiku 4.5', 'inferenceTypesSupported': ['ON_DEMAND']},
                {'modelId': 'anthropic.claude-opus-5-5-v1:0', 'modelName': 'Claude Opus 5.5', 'inferenceTypesSupported': ['INFERENCE_PROFILE']}]})
        if parsed.path == '/inference-profiles':
            server.record({'kind': 'inference-profiles', 'query': query, 'who': who})
            if 'nextToken' not in query:
                return self.reply(200, {'inferenceProfileSummaries': [
                    {'inferenceProfileId': 'us.anthropic.claude-opus-5-5-v1:0', 'inferenceProfileName': 'US Claude Opus 5.5', 'status': 'ACTIVE'},
                    {'inferenceProfileId': 'us.meta.llama4-v1:0', 'inferenceProfileName': 'US Llama', 'status': 'ACTIVE'}],
                    'nextToken': 'page/2'})
            return self.reply(200, {'inferenceProfileSummaries': [
                {'inferenceProfileId': 'global.anthropic.claude-sonnet-5-5-v1:0', 'inferenceProfileName': 'Global Sonnet 5.5', 'status': 'ACTIVE'}]})
        self.reply(404, {'message': 'unknown'})

    def do_POST(self):
        server = self.server
        raw = self.body()
        parsed = urllib.parse.urlsplit(self.path)
        if parsed.path == '/token':
            return self.token(raw)
        if parsed.path == '/':
            return self.sts(raw)
        if parsed.path.startswith('/v1/projects/'):
            return self.vertex(parsed.path, raw)
        if parsed.path.startswith('/model/'):
            return self.bedrock(parsed.path, raw)
        self.reply(404, {'message': 'unknown'})

    def sts(self, raw):
        server = self.server
        who, reason = verify_sigv4('POST', self.path, self.headers, raw, server.sts_secrets, 'sts')
        form = dict(urllib.parse.parse_qsl(raw.decode()))
        server.record({'kind': 'sts', 'form': form, 'who': who})
        if reason:
            server.fail(f'STS: {reason}')
            return self.reply(403, b'<ErrorResponse><Error><Code>SignatureDoesNotMatch</Code><Message>bad</Message></Error></ErrorResponse>', 'text/xml')
        server.secrets['ASIAASSUMED'] = 'assumedsecret'
        xml = ('<AssumeRoleResponse><AssumeRoleResult><Credentials><AccessKeyId>ASIAASSUMED</AccessKeyId>'
               '<SecretAccessKey>assumedsecret</SecretAccessKey><SessionToken>assumedtoken</SessionToken>'
               '<Expiration>2099-01-01T00:00:00Z</Expiration></Credentials></AssumeRoleResult></AssumeRoleResponse>')
        self.reply(200, xml.encode(), 'text/xml')

    def bedrock(self, path, raw):
        server = self.server
        who, reason = self.aws_auth(raw)
        stream = path.endswith('/invoke-with-response-stream')
        model = urllib.parse.unquote(path.split('/')[2])
        try:
            body = json.loads(raw)
        except ValueError:
            body = {}
        server.record({'kind': 'bedrock', 'stream': stream, 'model': model, 'body': body, 'who': who,
                       'token': self.headers.get('x-amz-security-token'), 'accept': self.headers.get('Accept')})
        if reason:
            server.fail(f'bedrock: {reason}')
            return self.reply(403, {'message': reason}, headers={'x-amzn-ErrorType': 'InvalidSignatureException:http://internal'})
        if 'model' in body or 'stream' in body or body.get('anthropic_version') != 'bedrock-2023-05-31':
            server.fail(f'bedrock body shape: {sorted(body)}')
        if server.scenario == 'throttle':
            return self.reply(429, {'message': 'Too many requests, please wait before trying again.'},
                              headers={'x-amzn-ErrorType': 'ThrottlingException:http://internal.amazon.com/coral/com.amazon.bedrock/',
                                       'x-amzn-RequestId': 'req-throttle'})
        if server.scenario == 'expire-once' and server.count('bedrock') == 1:
            return self.reply(403, {'message': 'The security token included in the request is expired'},
                              headers={'x-amzn-ErrorType': 'ExpiredTokenException:http://internal'})
        events = self.conversation(body, model)
        if not stream:
            return self.reply(200, message_json(answer_events(model)), headers={'x-amzn-RequestId': 'req-invoke'})
        frames = [es_chunk(e) for e in events]
        if server.scenario == 'midstream-exception':
            frames = frames[:3] + [es_exception('modelStreamErrorException', 'model hiccup')]
        # Split frames across writes so the client sees partial messages.
        blob = b''.join(frames)
        pieces = [blob[i:i + 37] for i in range(0, len(blob), 37)]
        self.stream('application/vnd.amazon.eventstream', pieces,
                    {'x-amzn-RequestId': 'req-stream', 'X-Amzn-Bedrock-Content-Type': 'application/json'})

    def token(self, raw):
        server = self.server
        form = dict(urllib.parse.parse_qsl(raw.decode()))
        server.record({'kind': 'token', 'grant': form.get('grant_type')})
        if form.get('grant_type') == 'urn:ietf:params:oauth:grant-type:jwt-bearer':
            verified = verify_rs256(form.get('assertion', ''), *server.rsa) if server.rsa else None
            if not verified:
                server.fail('JWT signature did not verify')
                return self.reply(400, {'error': 'invalid_grant', 'error_description': 'Invalid JWT Signature.'})
            head, claims = verified
            problems = []
            if head.get('alg') != 'RS256' or head.get('kid') != 'key-1':
                problems.append(f'header {head}')
            if claims.get('aud') != f'{server.url}/token' or claims.get('scope') != 'https://www.googleapis.com/auth/cloud-platform':
                problems.append(f'claims {claims}')
            if claims.get('iss') != 'bot@project.iam.gserviceaccount.com' or claims.get('exp', 0) - claims.get('iat', 0) != 3600:
                problems.append(f'issuer/lifetime {claims}')
            if problems:
                server.fail('; '.join(problems))
                return self.reply(400, {'error': 'invalid_grant'})
        elif form.get('grant_type') == 'refresh_token':
            if form.get('refresh_token') != server.refresh_token or form.get('client_id') != 'client-1':
                server.fail(f'refresh form {form}')
                return self.reply(400, {'error': 'invalid_grant'})
        else:
            server.fail(f'unknown grant {form}')
            return self.reply(400, {'error': 'unsupported_grant_type'})
        token = f'ya29.fake-{len(server.tokens_issued) + 1}'
        server.tokens_issued.append(token)
        self.reply(200, {'access_token': token, 'expires_in': 3600, 'token_type': 'Bearer'})

    def vertex(self, path, raw):
        server = self.server
        stream = path.endswith(':streamRawPredict')
        parts = path.split('/')
        auth = self.headers.get('Authorization', '')
        try:
            body = json.loads(raw)
        except ValueError:
            body = {}
        model = parts[-1].rsplit(':', 1)[0]
        server.record({'kind': 'vertex', 'stream': stream, 'project': parts[3], 'region': parts[5], 'model': model,
                       'auth': auth, 'body': body, 'beta': self.headers.get('anthropic-beta')})
        token = auth[len('Bearer '):] if auth.startswith('Bearer ') else ''
        allowed = set(server.tokens_issued) | {server.metadata_token, server.bearer}
        if server.scenario == 'unauth-once' and server.count('vertex') == 1:
            return self.reply(401, {'error': {'code': 401, 'message': 'Request had invalid authentication credentials.', 'status': 'UNAUTHENTICATED'}})
        if not token or token not in allowed:
            server.fail(f'vertex token {token!r} not issued')
            return self.reply(401, {'error': {'code': 401, 'message': 'bad token', 'status': 'UNAUTHENTICATED'}})
        if 'model' in body or body.get('anthropic_version') != 'vertex-2023-10-16' or body.get('stream') is not stream:
            server.fail(f'vertex body shape: {sorted(body)} stream={body.get("stream")}')
        if server.scenario == 'quota':
            return self.reply(429, {'error': {'code': 429, 'message': 'Quota exceeded for aiplatform.googleapis.com', 'status': 'RESOURCE_EXHAUSTED'}})
        if not stream:
            return self.reply(200, message_json(answer_events(model)))
        events = self.conversation(body, model)
        frames = [f'event: {e["type"]}\ndata: {json.dumps(e)}\n\n'.encode() for e in events]
        frames.insert(1, b'event: ping\ndata: {"type": "ping"}\n\n')
        if server.scenario == 'overloaded-midstream':
            frames = frames[:3] + [b'event: error\ndata: {"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}}\n\n']
        self.stream('text/event-stream', frames)
