#!/usr/bin/env python3
import argparse
import http.server
import json
import os
from pathlib import Path
import subprocess
import threading

class Search(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        assert 'q=useful%20query' in self.path,self.path
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
        calls=[{'name':'tools/list'},{'name':'browser_open','arguments':{'session_id':'durable'}},
               {'name':'browser_save_state','arguments':{'session_id':'durable','path':'/tmp/session.json'}},
               {'name':'tool_error','arguments':{}}]
        result=run([args.probe,'session','-'],env,'\n'.join(map(json.dumps,calls))+'\n')
        assert result.returncode==0,result.stderr
        rows=list(map(json.loads,result.stdout.splitlines()))
        assert rows[0]['tools'][0]['name']=='make_request'
        assert json.loads(rows[1]['content'][0]['text'])=={'name':'browser_open','arguments':{'session_id':'durable'}}
        assert json.loads(rows[2]['content'][0]['text'])=={'name':'browser_save_state','arguments':{'session_id':'durable','path':'/tmp/session.json'}}
        assert rows[3]['isError'] is True,'Tool errors must reach the agent intact'
        result=run([args.probe,'session','-'],env,json.dumps({'name':'disconnect'})+'\n')
        assert result.returncode==1,result.stderr
        assert 'disconnected' in result.stderr or 'ended before' in result.stderr,result.stderr
        env['BATBOX_CONNECTOR_TIMEOUT_MS']='150'
        result=run([args.probe,'session','-'],env,json.dumps({'name':'hang'})+'\n')
        assert result.returncode==1 and 'timed out' in result.stderr,result.stderr
        print('Native search filtering, MCP framing, persistent calls, errors, disconnect and timeout passed')
    finally:server.shutdown();server.server_close()

if __name__=='__main__':main()
