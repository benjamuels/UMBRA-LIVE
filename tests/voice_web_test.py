"""Run the editor's real transliterator; never silently drop a native lyric."""
from pathlib import Path
import re,subprocess
s=Path('web/editor.html').read_text()
a=s.index('function lyricText(')
fn=s[a:s.index('\n}',a)+2]
js="import assert from 'node:assert/strict';\n"+fn+'''
assert.equal(lyricText('Привет мир','RU'),'Privet mir'.toLowerCase());
assert.equal(lyricText('щёлк','RU'),'shchyolk');
assert.equal(lyricText('мой сыр','RU'),'moj syr');
assert.equal(lyricText('größer','DE'),'groesser');
assert.equal(lyricText('nǐ hǎo lǜ','ZH'),'ni hao lv');
assert.equal(lyricText('coração','PT'),'corasa~o');
assert.equal(lyricText('pytũ ñ','GN'),'pytu~ ny');
assert.equal(lyricText('été chat','FR'),'eitei chat');
assert.throws(()=>lyricText('你好','ZH'),/pinyin/);
assert.throws(()=>lyricText('щ'.repeat(16),'RU'),/63/);
assert.equal(lyricText('niño corazón'),'ninyo corazon');
assert.equal(lyricText('a'.repeat(100)).length,63); // legacy protocol bound
assert.equal(lyricText('a'.repeat(63),'EN').length,63);
console.log('voice web: transliteration, nasal/umlaut markers, UTF rejection and expanded-length bound passed');
'''
out=Path('build/song-check');out.mkdir(parents=True,exist_ok=True)
(out/'voice-web.mjs').write_text(js)
subprocess.run(['node',str(out/'voice-web.mjs')],check=True)
