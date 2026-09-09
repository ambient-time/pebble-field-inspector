'use strict';
var assert = require('assert');
var protocol = require('../src/pkjs/protocol');
function setup() {
  var messages=[],requests=[],timers=[],aborted=0;
  var client=protocol.createClient({
    send:function(p,done){messages.push(p);done();},
    request:function(method,url,body,done){requests.push({method:method,url:url,body:body,done:done});return function(){aborted++;};},
    setTimer:function(fn,ms){var t={fn:fn,ms:ms,cancelled:false};timers.push(t);return t;},
    clearTimer:function(t){t.cancelled=true;}
  });
  return {client:client,messages:messages,requests:requests,timers:timers,aborted:function(){return aborted;}};
}
function ask(h,id){h.client.handle({RequestId:id,RequestType:'ask',Prompt:'What changed?'});}
function latest(h,path){var a=h.requests.filter(function(r){return r.url.indexOf(protocol.BASE+path)===0;});return a[a.length-1];}
function ready(h,id,text){ask(h,id);latest(h,'start').done(null,{});latest(h,'status').done(null,{state:'ready',text:text||'A readable report.'});}
var count=0;
function test(name,fn){fn();count++;console.log('PASS '+name);}
test('capabilities whitelist excludes every credential',function(){
  var h=setup();h.client.sync();latest(h,'capabilities').done(null,{configured:true,enabled:['watch.battery'],confirmTranscript:true,key:'never-send',endpoint:'secret'});
  assert.deepStrictEqual(Object.keys(h.messages[0]).sort(),['BridgeReady','Configured','ConfirmTranscript','Enabled','ReducedMotion']);
  assert(!JSON.stringify(h.messages).includes('never-send'));
});
test('only reserved native URLs and no Authorization argument',function(){
  var h=setup();ready(h,1);h.client.handle({RequestId:1,TextAck:1});
  h.requests.forEach(function(r){assert(r.url.startsWith(protocol.BASE));assert(!r.token);});
});
test('working poll waits500ms and requires application acknowledgement',function(){
  var h=setup();ask(h,1);latest(h,'start').done(null,{});latest(h,'status').done(null,{state:'working',status:'Surveying'});
  assert.strictEqual(h.timers[0].ms,500);h.timers[0].fn();latest(h,'status').done(null,{state:'ready',text:'Ready'});
  assert(!latest(h,'delivered'));h.client.handle({RequestId:1,TextAck:1});assert(latest(h,'delivered'));
  latest(h,'delivered').done(null,{});h.client.handle({RequestId:1,TextAck:1});
  assert.strictEqual(h.requests.filter(function(r){return r.url.endsWith('/delivered');}).length,1);
});
test('missing watch ACK retransmits without another provider call',function(){
  var h=setup();ready(h,1);h.timers[h.timers.length-1].fn();
  assert.strictEqual(h.messages.filter(function(m){return m.ResponseText;}).length,2);
  assert.strictEqual(h.requests.filter(function(r){return r.url.endsWith('/start');}).length,1);
});
test('native delivery acknowledgement retries idempotently',function(){
  var h=setup();ready(h,1);h.client.handle({RequestId:1,TextAck:1});latest(h,'delivered').done('temporary');
  h.timers[h.timers.length-1].fn();assert.strictEqual(h.requests.filter(function(r){return r.url.endsWith('/delivered');}).length,2);
});
test('cancel invalidates late responses and invokes native cancellation',function(){
  var h=setup();ask(h,1);var old=latest(h,'start');h.client.handle({RequestId:1,RequestType:'cancel'});old.done(null,{});
  assert(latest(h,'cancel'));assert(!latest(h,'status'));assert.strictEqual(h.messages.length,0);
});
test('native cancel does not create cancellation feedback loop',function(){
  var h=setup();ask(h,1);h.client.configuration({kind:'cancel',request_id:1});
  assert(!latest(h,'cancel'));assert.strictEqual(h.messages[0].Command,'cancel');
  h.client.handle({RequestId:1,RequestType:'cancel'});assert(!latest(h,'cancel'));
});
test('old cancel cannot cancel a newer turn',function(){
  var h=setup();ask(h,1);ask(h,2);var oldCount=h.requests.length;h.client.handle({RequestId:1,RequestType:'cancel'});assert.strictEqual(h.requests.length,oldCount);
});
test('duplicate watch request never starts another inference',function(){
  var h=setup();ask(h,1);ask(h,1);assert.strictEqual(h.requests.length,1);
});
test('phone record transitions same ID to one confirmed question',function(){
  var h=setup();h.client.configuration({kind:'record',request_id:45,enabled:[]});
  assert.strictEqual(h.messages[0].Command,'record');assert(!latest(h,'status'));ask(h,45);ask(h,45);
  assert.strictEqual(h.requests.filter(function(r){return r.url.endsWith('/start');}).length,1);
  assert.deepStrictEqual(latest(h,'start').body,{kind:'ask',request_id:45,prompt:'What changed?'});
});
test('survey config preserves active start rather than cancelling it',function(){
  var h=setup();h.client.handle({RequestId:9,RequestType:'survey'});h.client.configuration({kind:'survey',request_id:9,enabled:['health.sleep']});
  assert(!latest(h,'cancel'));assert.strictEqual(h.messages[0].Enabled,'["health.sleep"]');
});
test('typed phone ask attaches polling without starting another inference',function(){
  var h=setup();h.client.configuration({kind:'ask',request_id:22});
  assert.strictEqual(h.messages[0].Command,'ask');assert(latest(h,'status'));assert(!latest(h,'start'));
  latest(h,'status').done(null,{state:'ready',text:'From the phone.'});
  assert.strictEqual(h.messages[1].ResponseText,'From the phone.');
});
test('serial AppMessage queue drops packets from invalidated generations',function(){
  var vm=require('vm'),fs=require('fs'),options,sent=[],callbacks=[];
  var fake={BASE:protocol.BASE,createClient:function(x){options=x;return {};}};
  vm.runInNewContext(fs.readFileSync(require('path').join(__dirname,'../src/pkjs/index.js'),'utf8'),{require:function(){return fake;},Pebble:{addEventListener:function(){},sendAppMessage:function(p,done){sent.push(p);callbacks.push(done);}}});
  options.send({RequestId:1},function(){},function(){},function(){return true;});
  options.send({RequestId:2,Command:'survey'},function(){},function(){},function(){return false;});
  options.send({RequestId:3},function(){},function(){},function(){return true;});
  callbacks[0]();assert.deepStrictEqual(sent.map(function(p){return p.RequestId;}),[1,3]);
});
test('watch observations preserve dates and timestamps and serialize completion',function(){
  var h=setup();h.client.handle({RequestId:3,RequestType:'survey'});
  var o={key:'health.sleep',source:'watch',value:600,unit:'seconds',collectedAt:1700000000000,measuredAt:1699999999000,date:'2026-09-07',period:'day'};
  h.client.handle({RequestId:3,Snapshot:JSON.stringify([o]),Complete:0});h.client.handle({RequestId:3,Snapshot:'[]',Complete:1});
  assert.strictEqual(h.requests.filter(function(r){return r.url.endsWith('/watch-data');}).length,1);
  assert.deepStrictEqual(latest(h,'watch-data').body.observations,[o]);latest(h,'watch-data').done(null,{});
  assert.strictEqual(latest(h,'watch-data').body.complete,true);
});
test('bad or cross-turn observation packets never upload',function(){
  var h=setup();ask(h,1);h.client.handle({RequestId:2,Snapshot:'[]'});assert(!latest(h,'watch-data'));
  h.client.handle({RequestId:1,Snapshot:'not json'});assert(!latest(h,'watch-data'));assert(h.messages[0].Complete);
});
test('900-byte UTF8 boundary includes multilingual responses safely',function(){
  var h=setup();ready(h,1,'界'.repeat(300));assert(h.messages[0].ResponseText);
  h=setup();ready(h,1,'界'.repeat(301));assert(!h.messages[0].ResponseText);assert(h.messages[0].Complete);
  assert.strictEqual(protocol.utf8Bytes('😀'),4);assert.strictEqual(protocol.utf8Bytes('\ud800'),Infinity);
});
test('empty and over400byte questions make no native request',function(){
  ['', '界'.repeat(134)].forEach(function(text){var h=setup();h.client.handle({RequestId:1,RequestType:'ask',Prompt:text});assert.strictEqual(h.requests.length,0);});
});
test('clear invalidates previous reply and native history',function(){
  var h=setup();ask(h,1);var old=latest(h,'start');h.client.handle({RequestType:'clear'});old.done(null,{});
  assert(latest(h,'clear'));assert(!latest(h,'status'));
});
test('missing bridge produces setup error, not relay fallback',function(){
  var h=setup();ask(h,1);latest(h,'start').done('Matching companion required');
  assert.strictEqual(h.messages[0].StatusText,'Matching companion required');assert.strictEqual(h.requests.length,1);
});
test('runtime receives native configmessage data and responds',function(){
  var vm=require('vm'),fs=require('fs'),events={},configured,responded;
  var fake={BASE:protocol.BASE,createClient:function(){return {configuration:function(x){configured=x;},sync:function(){},settings:function(){},handle:function(){}};}};
  vm.runInNewContext(fs.readFileSync(require('path').join(__dirname,'../src/pkjs/index.js'),'utf8'),{require:function(){return fake;},Pebble:{addEventListener:function(k,v){events[k]=v;}}});
  events.configmessage({data:JSON.stringify({kind:'record',request_id:8}),respond:function(v){responded=v;}});
  assert.strictEqual(configured.request_id,8);assert.strictEqual(responded.accepted,true);
});
test('capture works without an answer provider and starts only collection',function(){
  var h=setup();h.client.sync();latest(h,'capabilities').done(null,{configured:false,enabled:[]});
  assert.strictEqual(h.messages[0].BridgeReady,1);assert.strictEqual(h.messages[0].Configured,0);
  h.client.handle({RequestId:70,RequestType:'capture'});
  assert.strictEqual(latest(h,'start').body.kind,'capture');assert.strictEqual(latest(h,'start').body.prompt,undefined);
  h.client.configuration({kind:'capture',request_id:70,enabled:['watch.battery']});
  assert.strictEqual(h.messages[1].Command,'capture');assert(!latest(h,'cancel'));
  latest(h,'start').done(null,{});latest(h,'status').done(null,{state:'ready',text:'Saved 1 reading.'});
  h.client.handle({RequestId:70,TextAck:1});assert(latest(h,'delivered'));
});
test('history is a read-only bounded fetch with local acknowledgement',function(){
  var h=setup();h.client.handle({RequestId:71,RequestType:'history'});h.client.handle({RequestId:71,RequestType:'history'});
  assert.strictEqual(h.requests.length,1);assert.strictEqual(latest(h,'history').method,'GET');
  latest(h,'history').done(null,{text:'Sep 9: Saved readings'});h.client.handle({RequestId:71,TextAck:1});
  assert(!latest(h,'start'));assert(!latest(h,'delivered'));
  h.client.handle({RequestId:71,RequestType:'cancel'});assert(!latest(h,'cancel'));
});
test('cancelled history cannot replace a new result',function(){
  var h=setup();h.client.handle({RequestId:72,RequestType:'history'});var old=latest(h,'history');
  h.client.handle({RequestId:72,RequestType:'cancel'});old.done(null,{text:'Stale'});assert.strictEqual(h.messages.length,0);
});
test('history rejects oversized UTF8 and missing bridge disables capture affordance',function(){
  var h=setup();h.client.handle({RequestId:73,RequestType:'history'});latest(h,'history').done(null,{text:'界'.repeat(301)});
  assert(!h.messages[0].ResponseText);assert(h.messages[0].Complete);
  h=setup();h.client.sync();latest(h,'capabilities').done('Unavailable');assert.strictEqual(h.messages[0].BridgeReady,0);
});
test('phone settings refresh capabilities without starting or cancelling work',function(){
  var h=setup();ask(h,80);h.client.configuration({kind:'refresh'});
  latest(h,'capabilities').done(null,{configured:false,enabled:['watch.battery']});
  assert.strictEqual(h.messages[0].Configured,0);assert.strictEqual(h.messages[0].BridgeReady,1);
  assert(!latest(h,'cancel'));assert.strictEqual(h.requests.filter(function(r){return r.url.endsWith('/start');}).length,1);
});
console.log(count+' Signal Station protocol tests passed.');
