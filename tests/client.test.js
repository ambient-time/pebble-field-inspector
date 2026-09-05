'use strict';
var assert = require('assert');
var protocol = require('../src/pkjs/protocol');
function setup(settings) {
  var messages = [], requests = [], timers = [], aborted = 0;
  var client = protocol.createClient({
    settings:function () { return settings || {ClientToken:'test-installation-only'}; },
    send:function (packet, done) { messages.push(packet); done(); },
    fetchJson:function (url, token, body, done) { requests.push({url:url, token:token, body:body, done:done}); return function () { aborted++; }; },
    setTimer:function (fn) { var timer = {fn:fn, cancelled:false}; timers.push(timer); return timer; },
    clearTimer:function (timer) { timer.cancelled = true; }
  });
  return {client:client, messages:messages, requests:requests, timers:timers, aborted:function () { return aborted; }};
}
function inspect(h, id) { h.client.handle({RequestType:'inspect',RequestId:id,Prompt:'What is this?',SpeakerAvailable:1,Muted:0}); }
function reply(h, i, id, audio, extra) {
  h.requests[i].done(null,Object.assign({request_id:id,text:'A short field report.',audio:audio ? {pcm_base64:Buffer.from(audio).toString('base64'),sample_rate:8000,format:'s8'} : null},extra || {}));
}
function ack(h, id, seq) { h.client.handle({RequestId:id,AudioAck:seq}); }
var tests = 0;
function test(name, fn) { fn(); tests++; console.log('PASS '+name); }

test('settings never transmit endpoint or token',function () {
  var h=setup(); h.client.sync(); assert.deepStrictEqual(Object.keys(h.messages[0]).sort(),['Configured','VoiceEnabled','Volume']);
  assert(!JSON.stringify(h.messages).includes('test-installation'));
  assert.strictEqual(protocol.normalizeSettings({Endpoint:'http://localhost/inspect',ClientToken:'x'}).valid,false);
  assert.strictEqual(protocol.normalizeSettings({Endpoint:'https://user:pass@example.com/inspect',ClientToken:'x'}).valid,false);
  assert.strictEqual(protocol.normalizeSettings({ClientToken:{value:'x'},VoiceEnabled:{value:false},Volume:{value:900}}).volume,100);
});
test('invalid setup cannot make network request',function () {
  var h=setup({}); inspect(h,1); assert.strictEqual(h.requests.length,0); assert(/token/.test(h.messages[0].StatusText));
});
test('stale results and cancellation never deliver',function () {
  var h=setup(); inspect(h,1); inspect(h,2); reply(h,0,1); assert.strictEqual(h.messages.length,0);
  h.client.handle({RequestType:'cancel',RequestId:1}); reply(h,1,2); assert.strictEqual(h.messages[0].RequestId,2);
  inspect(h,3); h.client.handle({RequestType:'cancel',RequestId:3}); reply(h,2,3); assert.strictEqual(h.messages.length,1);
  assert(h.aborted()>=2);
});
test('server request ID and bounded text are checked',function () {
  var h=setup(); inspect(h,1); reply(h,0,99); assert(/Invalid/.test(h.messages[0].StatusText));
  var unicode=setup(); inspect(unicode,2); reply(unicode,0,2,null,{text:'界'.repeat(240)}); assert.strictEqual(unicode.messages[0].ResponseText.length,240);
  var emoji=setup(); inspect(emoji,4); reply(emoji,0,4,null,{text:'😀'.repeat(240)}); assert.strictEqual(emoji.messages[0].ResponseText.length,480);
  var large=setup(); inspect(large,3); reply(large,0,3,null,{text:'x'.repeat(241)}); assert(/Invalid/.test(large.messages[0].StatusText));
});
test('text arrives before begin, chunks wait for exact ACK, end is separate',function () {
  var h=setup(), audio=Buffer.alloc(1200); for(var i=0;i<audio.length;i++) audio[i]=i&255;
  inspect(h,42); reply(h,0,42,audio);
  assert(h.messages[0].ResponseText); assert.strictEqual(h.messages[1].AudioBegin,1200);
  ack(h,41,0); assert.strictEqual(h.messages.length,2); ack(h,42,9); assert.strictEqual(h.messages.length,2);
  ack(h,42,0); assert.strictEqual(h.messages[2].AudioChunk.length,512);
  ack(h,42,0); assert.strictEqual(h.messages.length,3);
  ack(h,42,1); ack(h,42,2); assert.strictEqual(h.messages[4].AudioChunk.length,176);
  ack(h,42,3); assert.strictEqual(h.messages[5].AudioEnd,1); ack(h,42,4);
  var bytes=[].concat(h.messages[2].AudioChunk,h.messages[3].AudioChunk,h.messages[4].AudioChunk); assert.deepStrictEqual(Buffer.from(bytes),audio);
});
test('missing ACK retries identical packet then bounded failure',function () {
  var h=setup(); inspect(h,1); reply(h,0,1,Buffer.alloc(600));
  for(var i=0;i<3;i++) h.timers[h.timers.length-1].fn();
  assert.strictEqual(h.messages.filter(function(x){return x.AudioBegin;}).length,3);
  assert(/timed out/.test(h.messages[h.messages.length-1].StatusText));
});
test('cancel prevents audio retries and stale ACKs',function () {
  var h=setup(); inspect(h,1); reply(h,0,1,Buffer.alloc(600)); var timer=h.timers[0];
  h.client.handle({RequestType:'cancel',RequestId:1}); timer.fn(); ack(h,1,0); assert.strictEqual(h.messages.length,2);
});
test('muted and speakerless watch request text only',function () {
  var h=setup(); h.client.handle({RequestType:'inspect',RequestId:1,Prompt:'Hello',SpeakerAvailable:0,Muted:0});
  assert.strictEqual(h.requests[0].body.speak,false); reply(h,0,1,Buffer.alloc(10)); assert.strictEqual(h.messages.length,1); assert.strictEqual(h.messages[0].AudioExpected,0);
  var m=setup(); m.client.handle({RequestType:'inspect',RequestId:2,Prompt:'Hello',SpeakerAvailable:1,Muted:1}); assert.strictEqual(m.requests[0].body.speak,false);
});
test('bad audio keeps valid text with visible warning',function () {
  var h=setup(); inspect(h,1); reply(h,0,1,null,{audio:{sample_rate:24000,format:'mp3',pcm_base64:'AAAA'}});
  assert.strictEqual(h.messages[0].ResponseText,'A short field report.'); assert.strictEqual(h.messages[0].AudioExpected,0); assert(/format/.test(h.messages[0].StatusText));
  assert.throws(function(){protocol.decodeAudio({sample_rate:8000,format:'s8',pcm_base64:'%%%%'});});
  assert.throws(function(){protocol.decodeAudio({sample_rate:8000,format:'s8',pcm_base64:Buffer.alloc(128001).toString('base64')});});
  assert.strictEqual(protocol.decodeAudio({sample_rate:8000,format:'s8',pcm_base64:Buffer.alloc(128000).toString('base64')}).length,128000);
});
test('offline demo and replay never call a model',function () {
  var h=setup({}); h.client.handle({RequestType:'demo',RequestId:1,SpeakerAvailable:1,Muted:0});
  assert.strictEqual(h.requests.length,0); assert.strictEqual(h.messages[0].Demo,1); assert(/tone, not a spoken/.test(h.messages[0].ResponseText));
  h.client.handle({RequestType:'replay',RequestId:2,SpeakerAvailable:0,Muted:0}); assert.strictEqual(h.requests.length,0); assert.strictEqual(h.messages[2].Demo,1);
});
test('actual Clay roundtrip keeps credentials on phone and sends no watch messages',function () {
  var pkg=require('../package.json'); assert.strictEqual(pkg.pebble.watchapp.watchface,false);
  assert(!pkg.pebble.messageKeys.includes('ClientToken')); assert(!pkg.pebble.messageKeys.includes('Endpoint'));
  var Module=require('module'), originalLoad=Module._load, previousPebble=global.Pebble;
  var previousStorage=Object.getOwnPropertyDescriptor(global,'localStorage');
  var keys={}, store={}, sent=[];
  pkg.pebble.messageKeys.forEach(function (name,i) { keys[name]=10000+i; });
  Module._load=function (id,parent,isMain) { return id==='message_keys' ? keys : originalLoad.call(this,id,parent,isMain); };
  Object.defineProperty(global,'localStorage',{configurable:true,writable:true,value:{getItem:function(k){return store[k]||null;},setItem:function(k,v){store[k]=v;}}});
  global.Pebble={addEventListener:function(){},sendAppMessage:function(p){sent.push(p);},getActiveWatchInfo:function(){return {platform:'emery'};}};
  try {
    var Clay=require('../src/pkjs/vendor/pebble-clay');
    var clay=new Clay(require('../src/pkjs/config.json'),null,{autoHandleEvents:false});
    var values={Endpoint:'https://example.com/v1/inspect',ClientToken:'restricted-installation-test-token',VoiceEnabled:false,Volume:40};
    clay.getSettings(encodeURIComponent(JSON.stringify(values)),false);
    var saved=JSON.parse(store['clay-settings']);
    Object.keys(values).forEach(function(k){assert.strictEqual(saved[k],values[k]);});
    assert.deepStrictEqual(sent,[]);
  } finally {
    Module._load=originalLoad; global.Pebble=previousPebble;
    if(previousStorage) Object.defineProperty(global,'localStorage',previousStorage); else delete global.localStorage;
  }
});
console.log(tests+' client protocol tests passed.');
