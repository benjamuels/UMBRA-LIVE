"""Use the editor's actual JS to check contextual descriptor refresh and naming."""
from pathlib import Path
import re, subprocess
s=Path('web/editor.html').read_text()
out=Path('build/song-check');out.mkdir(parents=True,exist_ok=True)
js='\n'.join(re.findall(r'<script(?:\s[^>]*)?>(.*?)</script>',s,re.S))
(out/'editor.mjs').write_text(js)
subprocess.run(['node','--check',str(out/'editor.mjs')],check=True)
def fn(name):
    a=re.search(r'(?:async )?function '+name+r'\(',js).start()
    return js[a:js.index('\n}',a)+2]
test='''import assert from 'node:assert/strict';
let builds=0, mode=0, calls=[];
let dev={info:{pe0:4,pcount:12,engines:['INPUT']},dump:{engine:0},built:0,pdesc:Array.from({length:12},(_,i)=>({label:i<4?(i===1?'DST2':'DEST'+(i+1)):'P'+i,names:['OFF']}))};
function assertSession(d){assert.equal(d,dev);}
async function getDescOrNull(s,i){calls.push(i);return {...dev.pdesc[i],names:["OFF",mode?'T:MOVE':'T:POS']};}
function buildGroups(){builds++;}
'''+fn('matrixContextId')+'\n'+fn('rereadEngineDesc')+'\n'+fn('modDst')+'\n'+fn('aliasOf')+'''
assert.equal(aliasOf(['OFF','V:AMP','OFF'],2),0);assert.equal(aliasOf(['OFF','V:AMP','T:LEVEL'],2),2);
assert(matrixContextId(dev,4));assert(matrixContextId(dev,7));assert(matrixContextId(dev,0));assert(!matrixContextId(dev,6));
await rereadEngineDesc(); assert.equal(calls.length,12);assert.equal(builds,1);
mode=1;calls=[];await rereadEngineDesc();assert.equal(calls.filter(i=>i<4).length,4);assert.equal(builds,2);
await rereadEngineDesc();assert.equal(builds,2);
dev.pdesc[4]={label:'MORPH'};
assert.equal(modDst({label:'DEST1'},'E1'),'MORPH');
assert.equal(modDst({label:'DST1'},'E1'),'MORPH');
assert.equal(modDst({label:'DEST1'},'T:MOVE'),'T:MOVE');
console.log('matrix web: JS syntax, current/legacy labels and contextual metadata refresh passed');
dev.info.pe0=80;dev.info.pcount=88;dev.pdesc=Array.from({length:88},(_,i)=>({label:'P'+i}));calls=[];
await rereadEngineDesc();
assert(calls.includes(61)&&calls.includes(74),'engine switches refresh shared language/key-map and lyric/image descriptors');
'''
# modDst uses the existing visibility predicate only for old E1..E8 labels.
test=test.replace('function assertSession', 'function visible(d){return d.label!=="-";}\nfunction assertSession')
(out/'matrix-web.mjs').write_text(test)
subprocess.run(['node',str(out/'matrix-web.mjs')],check=True)
assert 'disabled: /(?:^|:)N' in s
