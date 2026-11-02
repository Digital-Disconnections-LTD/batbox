#!/usr/bin/env python3
import argparse
import http.server
import json
import os
from pathlib import Path
import subprocess
import threading
from urllib.parse import parse_qs, urlsplit

FORBIDDEN='<!DOCTYPE html><html><head><title>403 Forbidden</title></head><body><h1>Forbidden</h1></body></html>'
# Query -> (status, content type, body) for answers that are not the shape Batbox reads.
ANSWERS={
    'json off':(403,'text/html; charset=utf-8',FORBIDDEN),
    'not json':(200,'text/html','<html><body>Sign in to continue</body></html>'),
    'no results':(200,'application/json',json.dumps({'query':'no results','answers':[]})),
    'no url':(200,'application/json',json.dumps({'results':[{'link':'https://x.example','title':'t'}]})),
    'null fields':(200,'application/json',json.dumps({'query':'null fields','number_of_results':None,
        'suggestions':None,'results':[
            {'url':'https://docs.example.org/a','title':None,'content':None,'engine':None,'publishedDate':None},
            {'title':'No url'},{'url':None},None,
            {'url':'https://docs.example.org/b','title':'Kept','content':'text'}]})),
}

class Search(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        query=parse_qs(urlsplit(self.path).query)
        assert query.get('format')==['json'],self.path
        q=query['q'][0]
        if q in ANSWERS:
            status,kind,body=ANSWERS[q]
            payload=body.encode()
            self.send_response(status);self.send_header('Content-Type',kind);self.end_headers();self.wfile.write(payload)
            return
        assert q=='useful query',self.path
        payload=json.dumps({'results':[
            {'title':'Allowed','url':'https://docs.example.org/read'},
            {'title':'Blocked','url':'https://private.example.org/read'},
            {'title':'Not a suffix match','url':'https://wrongexample.org/read'},
        ]}).encode()
        self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(payload)
    def log_message(self,*args):pass

def run(command,env,stdin=None):
    return subprocess.run(command,input=stdin,env=env,text=True,capture_output=True,timeout=8)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--probe',required=True);args=parser.parse_args()
    base=Path(__file__).resolve().parent
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Search)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    env=dict(os.environ,BATBOX_SEARXNG_URL=f'http://127.0.0.1:{server.server_port}/search',
        BATBOX_SCRAPLING_EXECUTABLE=str(base/'fake_scrapling.py'))
    try:
        request={'query':'useful query','allowed_domains':['example.org'],'blocked_domains':['private.example.org']}
        result=run([args.probe,'search','useful query',json.dumps(request)],env)
        assert result.returncode==0,result.stderr
        payload=json.loads(result.stdout)
        assert payload['query']=='useful query'
        assert payload['results']==[{'tool_use_id':'batbox-search','content':[{'title':'Allowed','url':'https://docs.example.org/read'}]}]
        result=run([args.probe,'search','null fields'],env)
        assert result.returncode==0,result.stderr
        assert json.loads(result.stdout)['results'][0]['content']==[
            {'title':'','url':'https://docs.example.org/a'},{'title':'Kept','url':'https://docs.example.org/b'}],result.stdout
        endpoint=env['BATBOX_SEARXNG_URL']
        for q,expected in (
            ('json off',[f'SearXNG at {endpoint}: expected HTTP 200 with JSON search results, got HTTP 403 '
                '(text/html; charset=utf-8): "<!DOCTYPE html><html><head><title>403 Forbidden</title>',
                'Fix: SearXNG answers 403 when search.formats in its settings.yml lacks json','batbox setup web']),
            ('not json',['expected a JSON body, got HTTP 200 (text/html): "<html><body>Sign in to continue</body></html>"']),
            ('no results',['expected a JSON object with a "results" array, got an object without "results" (keys: query, answers)']),
            ('no url',['expected each result to have a "url" string, got 1 result without one (first result: keys link, title)'])):
            result=run([args.probe,'search',q],env)
            assert result.returncode==1,result.stdout
            for part in expected:assert part in result.stderr,(q,result.stderr)
        calls=[{'name':'tools/list'},{'name':'browser_open','arguments':{'session_id':'durable'}},
               {'name':'browser_save_state','arguments':{'session_id':'durable','path':'/tmp/session.json'}},
               {'name':'tool_error','arguments':{}}]
        result=run([args.probe,'session','-'],env,'\n'.join(map(json.dumps,calls))+'\n')
        assert result.returncode==0,result.stderr
        rows=list(map(json.loads,result.stdout.splitlines()))
        assert [tool['name'] for tool in rows[0]['tools']][3]=='get'
        assert json.loads(rows[1]['content'][0]['text'])=={'name':'browser_open','arguments':{'session_id':'durable'}}
        assert json.loads(rows[2]['content'][0]['text'])=={'name':'browser_save_state','arguments':{'session_id':'durable','path':'/tmp/session.json'}}
        assert rows[3]['isError'] is True,'Tool errors must reach the agent intact'
        result=run([args.probe,'fetch','https://example.org/page'],env)
        assert result.returncode==0,result.stderr
        sent=json.loads(json.loads(result.stdout)['content'][0]['text'])
        assert sent=={'name':'get','arguments':{'url':'https://example.org/page','extraction_type':'markdown'}},sent
        result=run([args.probe,'fetch','https://example.org/page'],dict(env,FAKE_SCRAPLING_TOOLS='unknown'))
        assert result.returncode==1,result.stdout
        assert 'has no tool named make_request or get (it offers: browser_open, browser_fetch)' in result.stderr,result.stderr
        assert 'Batbox needs Scrapling >= 0.3. Run: batbox setup web' in result.stderr,result.stderr
        result=run([args.probe,'session','-'],env,json.dumps({'name':'disconnect'})+'\n')
        assert result.returncode==1,result.stderr
        assert 'disconnected' in result.stderr or 'ended before' in result.stderr,result.stderr
        env['BATBOX_CONNECTOR_TIMEOUT_MS']='150'
        result=run([args.probe,'session','-'],env,json.dumps({'name':'hang'})+'\n')
        assert result.returncode==1 and 'timed out' in result.stderr,result.stderr
        print('Native search filtering, SearXNG answer shapes, MCP framing, persistent calls, fetch tool choice, errors, disconnect and timeout passed')
    finally:server.shutdown();server.server_close()

if __name__=='__main__':main()
