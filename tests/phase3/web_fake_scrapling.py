#!/usr/bin/env python3
"""Minimal actual MCP transport fixture for the WebFetch Scrapling adapter."""
import json,os,sys
for line in sys.stdin:
 req=json.loads(line)
 if 'id' not in req:continue
 method=req['method']
 if method=='initialize': result={'protocolVersion':'2025-03-26','capabilities':{'tools':{}}}
 elif method=='tools/call':
  url=req['params']['arguments']['url']
  log=os.environ.get('BATBOX_WEB_FETCH_LOG')
  if log:
   with open(log,'a') as out:out.write(url+'\n')
  actual='https://else.example.org/final' if 'redirect.example.org' in url else url
  text='# Documentation' if 'docs.example.org' in url else '# Article\nA fact.'
  page={'status':200,'content':[text],'url':actual}
  result={'isError':False,'content':[{'type':'text','text':json.dumps(page)}]}
 else:result={'tools':[{'name':'make_request'}]}
 print(json.dumps({'jsonrpc':'2.0','id':req['id'],'result':result}),flush=True)
