#!/usr/bin/env python3
"""Bedrock event-stream decoding checks with frames built by an independent Python encoder."""
import argparse
import base64
import json
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cloud_fakes import es_chunk, es_error, es_exception, es_header, es_message, tool_use_events  # noqa: E402


def decode(probe, data, chunk):
    with tempfile.NamedTemporaryFile(delete=False) as handle:
        handle.write(data)
        path = handle.name
    try:
        process = subprocess.run([probe, 'eventstream', path, str(chunk)], capture_output=True, text=True, timeout=30)
        if process.returncode:
            raise AssertionError(process.stderr)
        return json.loads(process.stdout)
    finally:
        os.unlink(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    probe = parser.parse_args().probe
    events = tool_use_events()
    events[-1] = {'type': 'message_stop', 'amazon-bedrock-invocationMetrics': {'inputTokenCount': 12, 'outputTokenCount': 30}}
    stream = b''.join(es_chunk(e) for e in events)

    for chunk in (1, 3, 7, 64, 1000, len(stream)):
        result = decode(probe, stream, chunk)
        assert 'error' not in result, (chunk, result)
        assert result['events'] == events, (chunk, result['events'][:2])
        assert all(m['headers'] == {':event-type': 'chunk', ':content-type': 'application/json', ':message-type': 'event'}
                   for m in result['messages'])

    typed = es_message([es_header('t', 0), es_header('f', 1), es_header('byte', 2, -5), es_header('short', 3, 300),
                        es_header('int', 4, -70000), es_header('long', 5, 2 ** 40), es_header('bytes', 6, b'\x00\x01'),
                        es_header('string', 7, 'café'), es_header('time', 8, 1700000000000),
                        es_header('uuid', 9, bytes(range(16))), es_header(':message-type', 7, 'event'),
                        es_header(':event-type', 7, 'metadata')], b'{"ignored":true}')
    result = decode(probe, typed, 5)
    assert 'error' not in result and result['events'] == [], result
    assert result['messages'][0]['headers'] == {
        't': 'true', 'f': 'false', 'byte': '-5', 'short': '300', 'int': '-70000', 'long': str(2 ** 40),
        'bytes': base64.b64encode(b'\x00\x01').decode(), 'string': 'café', 'time': '1700000000000',
        'uuid': bytes(range(16)).hex(), ':message-type': 'event', ':event-type': 'metadata'}, result['messages'][0]['headers']
    assert result['messages'][0]['payload_bytes'] == 16

    first = es_chunk(events[0])
    corrupt = bytearray(first)
    corrupt[-6] ^= 0x01
    result = decode(probe, bytes(corrupt), 9)
    assert result['error']['kind'] == 'EventStreamError' and 'message CRC' in result['error']['message'], result
    corrupt = bytearray(first)
    corrupt[7] ^= 0x01
    result = decode(probe, bytes(corrupt) + stream, 64)
    assert result['error']['kind'] == 'EventStreamError' and 'prelude CRC' in result['error']['message'], result
    assert result['events'] == []

    throttled = es_chunk(events[0]) + es_chunk(events[1]) + es_exception('throttlingException', 'Too many requests')
    result = decode(probe, throttled, 11)
    assert result['events'] == events[:2], result
    error = result['error']
    assert error['status'] == 429 and error['body']['error']['type'] == 'rate_limit_error', error
    assert 'Too many requests' in error['body']['error']['message']

    for name, status, kind in (('validationException', 400, 'invalid_request_error'), ('modelStreamErrorException', 500, 'api_error'),
                               ('serviceUnavailableException', 503, 'overloaded_error'), ('modelTimeoutException', 408, 'api_error'),
                               ('internalServerException', 500, 'api_error')):
        result = decode(probe, es_exception(name, 'x'), 64)
        assert result['error']['status'] == status and result['error']['body']['error']['type'] == kind, (name, result)

    result = decode(probe, es_chunk(events[0]) + es_error('InternalFailure', 'boom'), 64)
    assert result['error']['status'] == 500 and 'boom' in result['error']['body']['error']['message'], result

    result = decode(probe, stream[:-5], 50)
    assert result['error']['kind'] == 'APIConnectionError', result
    assert result['events'] == events[:-1]

    overloaded = es_chunk({'type': 'error', 'error': {'type': 'overloaded_error', 'message': 'Overloaded'}})
    result = decode(probe, overloaded, 64)
    assert result['error']['kind'] == 'APIError' and 'status' not in result['error'], result
    assert result['error']['body']['error']['type'] == 'overloaded_error'

    bad = es_message([es_header(':event-type', 7, 'chunk'), es_header(':message-type', 7, 'event')], b'{"bytes":"%%%"}')
    result = decode(probe, bad, 64)
    assert result['error']['kind'] == 'EventStreamError' and 'base64' in result['error']['message'], result

    print('Bedrock event-stream framing, typed headers, CRC, exception, error-frame and truncation checks passed')


if __name__ == '__main__':
    main()
