// ภาพรวม: ทดสอบ JavaScript ของหน้าเว็บ MQTT ด้วย DOM จำลอง ไม่สั่งอุปกรณ์จริง
// Unit tests execute the actual inline dashboard JS, without browser automation.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const html=fs.readFileSync('web/index.html','utf8'),elements=new Map(),timeouts=new Map();let tid=0;
class Element{
 constructor(tag='div'){this.tagName=tag;this.children=[];this.value='';this.textContent='';this.style={};this.dataset={};this.clientWidth=900;this.checked=false;}
 set id(v){this._id=v;elements.set(v,this);}get id(){return this._id;}
 append(...v){this.children.push(...v);}replaceChildren(...v){this.children=v;}querySelectorAll(selector){return this.children.flatMap(x=>x instanceof Element?[...(selector==='button'&&x.tagName==='button'?[x]:[]),...x.querySelectorAll(selector)]:[]);}
 getContext(){return new Proxy({},{get:()=>()=>{}});}click(){}
}
for(const m of html.matchAll(/id="([^"]+)"/g)){const e=new Element();e.id=m[1];}elements.get('range').value='2';
const commands=[...html.matchAll(/data-cmd="([^"]+)"/g)].map(m=>{const e=new Element('button');e.dataset.cmd=m[1];return e;});
const document={getElementById:id=>elements.get(id),createElement:tag=>new Element(tag),createTextNode:s=>s,querySelectorAll:()=>commands};
const storage=new Map();const sandbox={document,console,TextEncoder,URL,Blob,devicePixelRatio:1,crypto:require('node:crypto').webcrypto,
 localStorage:{getItem:k=>storage.get(k),setItem:(k,v)=>storage.set(k,v)},confirm:()=>true,
 indexedDB:{open:()=>({})},setInterval:()=>0,setTimeout:(fn,ms)=>{timeouts.set(++tid,fn);return tid;},clearTimeout:id=>timeouts.delete(id)};
sandbox.window={addEventListener:()=>{}};const ctx=vm.createContext(sandbox),run=s=>vm.runInContext(s,ctx);
vm.runInContext([...html.matchAll(/<script(?: [^>]*)?>([\s\S]*?)<\/script>/g)].at(-1)[1],ctx);
(async()=>{
 assert.equal(run("crc32('123456789')"),'CBF43926');
 await assert.rejects(run("send('EXTEND')"),/offline/);
 await run('demo()');assert.equal(run('records.size'),96);assert.equal(run('available()'),true);
 run("transfer={chunks:new Map(),started:false};demoCommand('READLOG','cmd')");assert.equal(run('transfer'),null);assert.equal(elements.get('progress').value,1);
 run("transfer={chunks:new Map(),started:false};receive('log/file','#BEGIN rows=0 since=0');receive('log/file','#CHUNK 0\\n'+fields.join(',')+'\\n');receive('log/file','#END rows=0 crc32=00000000')");assert.match(elements.get('notice').textContent,/CRC32/);
 run("demoCommand('STOP','cmd');demoCommand('EXTEND','cmd')");assert.equal(run('latest.line'),'UNKNOWN');assert.match(elements.get('ack').textContent,/✗/);
 run("demoCommand('RETRACT','cmd');demoAngle=15;demoStepAt=0;demoTick()");assert.equal(run('latest.line'),'RETRACTED');
 run("demoCommand('LABEL 1 1','cmd')");assert.equal(run('records.get(1).rain_label'),1);
 run("demoCommand('DELLOG','cmd');transfer={chunks:new Map(),started:false};demoCommand('READLOG','cmd')");assert.equal(run('demoBoard.size'),0);assert.equal(run('records.size'),96);assert.equal(run('transfer'),null);
 const model=fs.readFileSync('training/model.json','utf8');ctx.modelText=model;run('modelValidate(JSON.parse(modelText),modelText)');
 assert.throws(()=>run("let bad=JSON.parse(modelText);bad.scale[0]=0;modelValidate(bad,JSON.stringify(bad))"),/scale/);
 run("receive('status',JSON.stringify({...latest,model:'<img src=x onerror=alert(1)>'}))");assert.match(elements.get('meta').textContent,/<img/); // textContent, never interpreted HTML
 run("cleanup()");await assert.rejects(run("send('STOP')"),/offline/);assert.equal(run('online'),false);
 console.log('PASS: dashboard demo, stop/retract, offline guard, labels, model schema, READLOG CRC, text rendering.');
})().catch(e=>{console.error(e);process.exitCode=1;});
