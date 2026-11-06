#!/usr/bin/env python3
import argparse,json,subprocess,tempfile,shutil
from pathlib import Path
def run(probe,item):
  p=subprocess.run([probe],input=json.dumps(item),text=True,capture_output=True,check=True,timeout=10)
  return json.loads(p.stdout)
def expected(args,target):
  p=subprocess.run(['rg',*args,target],text=True,capture_output=True,check=False,timeout=10)
  return [line.rstrip('\r') for line in p.stdout.strip().split('\n') if line.rstrip('\r')]
def main():
  ap=argparse.ArgumentParser();ap.add_argument('--probe',required=True);a=ap.parse_args()
  with tempfile.TemporaryDirectory(prefix='batbox-rg-') as directory:
    root=Path(directory);(root/'nested').mkdir()
    (root/'a.txt').write_text('alpha\nbeta\n')
    (root/'nested'/'b.txt').write_text('alpha\n')
    (root/'ignored.md').write_text('alpha\n')
    checks=[
      (['--files','--glob','*.txt','--sort=modified','--no-ignore','--hidden'],str(root)),
      (['--hidden','--glob','!.git','--max-columns','500','-l','alpha'],str(root)),
      (['--hidden','--max-columns','500','-n','alpha'],str(root)),
      (['--hidden','--max-columns','500','-c','alpha'],str(root)),
      (['--hidden','--max-columns','500','-l','unlikely'],str(root)),
    ]
    for index,(args,target) in enumerate(checks):
      actual=run(a.probe,{'args':args,'target':target})
      want={'result':expected(args,target)}
      if sorted(actual.get('result',[]))!=sorted(want['result']):
        print('Mismatch',index,args);print('rg:',want);print('native:',actual);return 1
    for kind,item in [
      ('missing',{'command':'/does/not/exist/rg','target':str(root)}),
      ('timeout',{'command':shutil.which('sleep'),'target':'2','timeoutMs':20}),
      ('cancel',{'command':shutil.which('sleep'),'target':'2','timeoutMs':1000,'stop':True}),
    ]:
      actual=run(a.probe,item)
      if 'error' not in actual:
        print('Expected error',kind,actual);return 1
      if kind!='missing' and 'timed out' not in actual['error']:
        print('Wrong cancellation mapping',kind,actual);return 1
    failures=0
    bad_regex=run(a.probe,{'args':['-n','unclosed('],'target':str(root)})
    if 'regex parse error' not in bad_regex.get('error',''):
      print('A bad regex must fail, not report no matches:',bad_regex);failures+=1
    missing=str(root/'not-here')
    partial_paths=run(a.probe,{'args':['-l','alpha',str(root/'nested')],'target':missing})
    result=partial_paths.get('result',{})
    if not isinstance(result,dict) or result.get('lines')!=[str(root/'nested'/'b.txt')] or \
       'Some paths could not be searched' not in result.get('incomplete','') or \
       'not-here' not in result.get('incomplete',''):
      print('Matches beside an unreadable path must carry the path error:',partial_paths);failures+=1
    no_match_paths=run(a.probe,{'args':['-l','zzz-absent'],'target':missing})
    result=no_match_paths.get('result',{})
    if not isinstance(result,dict) or result.get('lines')!=[] or 'not-here' not in result.get('incomplete',''):
      print('A path error with no matches must still be reported:',no_match_paths);failures+=1
    overflow=run(a.probe,{'args':['-n','a'],'target':str(root),'maxBufferBytes':12})
    result=overflow.get('result',{})
    if not isinstance(result,dict) or 'output passed the' not in result.get('incomplete',''):
      print('Output cut at the buffer limit must say so:',overflow);failures+=1
    slow=root/'slow-rg'
    slow.write_text('#!/bin/sh\nprintf "first\\nsecond\\nthird\\n"\nexec sleep 30\n')
    slow.chmod(0o755)
    # Long enough for the lines to arrive on a loaded machine; the search is still cut.
    timed=run(a.probe,{'command':str(slow),'target':'.','timeoutMs':3000})
    result=timed.get('result',{})
    if not isinstance(result,dict) or result.get('lines')!=['first','second'] or \
       'timed out after' not in result.get('incomplete',''):
      print('Lines cut short by the timeout must say so:',timed);failures+=1
    if failures:return 1
    print(f'{len(checks)+8} native ripgrep process cases passed');return 0
if __name__=='__main__':raise SystemExit(main())
