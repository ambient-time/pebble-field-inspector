'use strict';
// Phone-only protocol. No credentials are ever fields in a watch message.
var MAX_AUDIO = 128000;
var CHUNK = 512;
function characterCount(text) { return text.replace(/[\uD800-\uDBFF][\uDC00-\uDFFF]/g, 'x').length; }
var DEFAULT_ENDPOINT = 'https://api.dr.eamer.dev/pebble-inspector/v1/inspect';

function normalizeSettings(raw) {
  raw = raw || {};
  function value(key, fallback) {
    var v = raw[key];
    if (v && typeof v === 'object') v = v.value;
    return v === undefined ? fallback : v;
  }
  var endpoint = String(value('Endpoint', DEFAULT_ENDPOINT)).trim();
  var token = String(value('ClientToken', '')).trim();
  return {
    endpoint: endpoint,
    token: token,
    valid: /^https:\/\/[^\s\/?#:@]+(?::[0-9]+)?\/[^\s#]*$/.test(endpoint) && token.length > 0 && token.length <= 256,
    voice: value('VoiceEnabled', true) !== false && value('VoiceEnabled', true) !== 0,
    volume: Math.max(10, Math.min(100, Number(value('Volume', 65)) || 65))
  };
}

function decodeAudio(audio) {
  if (!audio) return null;
  if (audio.sample_rate !== 8000 || audio.format !== 's8') throw new Error('Unsupported voice format. Text is ready.');
  var b64 = audio.pcm_base64;
  if (typeof b64 !== 'string' || b64.length > Math.ceil(MAX_AUDIO / 3) * 4 || b64.length % 4 ||
      !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(b64)) {
    throw new Error('Invalid voice data. Text is ready.');
  }
  var alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var size = b64.length / 4 * 3 - (/==$/.test(b64) ? 2 : /=$/.test(b64) ? 1 : 0);
  if (size > MAX_AUDIO) throw new Error('Voice reply is too long. Text is ready.');
  var out = new Uint8Array(size), cursor = 0;
  for (var i = 0; i < b64.length; i += 4) {
    var n = (alphabet.indexOf(b64[i]) << 18) | (alphabet.indexOf(b64[i + 1]) << 12) |
      (Math.max(0, alphabet.indexOf(b64[i + 2])) << 6) | Math.max(0, alphabet.indexOf(b64[i + 3]));
    if (cursor < size) out[cursor++] = (n >>> 16) & 255;
    if (cursor < size) out[cursor++] = (n >>> 8) & 255;
    if (cursor < size) out[cursor++] = n & 255;
  }
  return out;
}

function createClient(options) {
  var active = null, generation = 0, cached = null;
  var later = options.setTimer || setTimeout, clear = options.clearTimer || clearTimeout;
  function valid(a) { return active === a && a.generation === generation; }
  function settings() { return normalizeSettings(options.settings()); }
  function send(packet, done, failed) { options.send(packet, done || function () {}, failed || function () {}); }
  function cancel() {
    generation++;
    var old = active;
    active = null;
    if (old) {
      if (old.timer) clear(old.timer);
      if (old.abort) old.abort();
    }
  }
  function failed(a, text) {
    if (!valid(a)) return;
    if (a.timer) clear(a.timer);
    a.waiting = null;
    send({RequestId:a.id, StatusText:String(text).slice(0,100), AudioExpected:0});
  }
  function transmit(a, packet) {
    if (!valid(a)) return;
    if (a.timer) clear(a.timer);
    a.waiting = packet;
    send(packet, function () {
      if (!valid(a) || a.waiting !== packet) return;
      a.timer = later(function () {
        if (!valid(a) || a.waiting !== packet) return;
        if (++a.retries > 2) return failed(a, 'Voice transfer timed out. Text is ready.');
        transmit(a, packet);
      }, 6000);
    }, function () { failed(a, 'Watch connection lost. Try again.'); });
  }
  function next(a) {
    if (!valid(a)) return;
    a.retries = 0;
    if (a.sequence === 0) {
      transmit(a, {RequestId:a.id, AudioBegin:a.audio.length, AudioSequence:0});
    } else if (a.offset < a.audio.length) {
      var end = Math.min(a.offset + CHUNK, a.audio.length);
      var bytes = [];
      for (var i = a.offset; i < end; i++) bytes.push(a.audio[i]);
      a.pendingEnd = end;
      transmit(a, {RequestId:a.id, AudioSequence:a.sequence, AudioChunk:bytes});
    } else {
      transmit(a, {RequestId:a.id, AudioSequence:a.sequence, AudioEnd:1});
    }
  }
  function deliver(a, result) {
    if (!valid(a)) return;
    a.audio = result.audio;
    a.offset = 0;
    a.sequence = 0;
    var warning = result.warning || '';
    var playable = a.speak && a.audio && a.audio.length;
    send({RequestId:a.id, ResponseText:result.text, Demo:result.demo ? 1 : 0,
      StatusText:warning || (result.demo ? 'OFFLINE DEMO' : 'Reply ready'), AudioExpected:playable ? 1 : 0}, function () {
        if (valid(a) && playable) next(a);
      }, function () { failed(a, 'Watch connection lost. Try again.'); });
  }
  function begin(payload) {
    cancel();
    var cfg = settings();
    var a = active = {id:payload.RequestId, generation:generation, timer:null, abort:null,
      speak:cfg.voice && !!payload.SpeakerAvailable && !payload.Muted, waiting:null};
    if (payload.RequestType === 'demo') {
      var audio = new Uint8Array(8000);
      for (var i = 0; i < audio.length; i++) {
        var fade = Math.min(1, i / 120, (audio.length - i) / 120);
        audio[i] = Math.round(Math.sin(i * 2 * Math.PI * 523.25 / 8000) * 32 * fade) & 255;
      }
      cached = {text:'OFFLINE DEMO\nA field report appears here. Real replies come from the question you confirm. This test plays a tone, not a spoken answer.', audio:audio, demo:true};
      return deliver(a, cached);
    }
    if (payload.RequestType === 'replay') {
      if (!cached) return failed(a, 'No saved reply. Select to ask, or try Demo in Help.');
      return deliver(a, cached);
    }
    if (!cfg.valid) return failed(a, 'Open phone settings and add an installation token.');
    if (typeof payload.Prompt !== 'string' || !payload.Prompt.trim() || characterCount(payload.Prompt) > 400) {
      return failed(a, 'Question is empty or too long. Please try again.');
    }
    var abort = options.fetchJson(cfg.endpoint, cfg.token,
      {request_id:a.id, prompt:payload.Prompt, speak:a.speak}, function (error, data) {
        if (!valid(a)) return;
        a.abort = null;
        if (error) return failed(a, error);
        if (!data || data.request_id !== a.id || typeof data.text !== 'string' || !data.text.trim() || characterCount(data.text) > 240) {
          return failed(a, 'Invalid service reply. Please try again.');
        }
        var pcm = null, warning = typeof data.warning === 'string' ? data.warning.slice(0,100) : '';
        try { if (a.speak) pcm = decodeAudio(data.audio); } catch (e) { warning = e.message; }
        cached = {text:data.text, audio:pcm, warning:warning, demo:false};
        deliver(a, cached);
      });
    if (valid(a)) a.abort = abort;
  }
  return {
    sync: function () {
      var cfg = settings();
      send({Configured:cfg.valid ? 1 : 0, VoiceEnabled:cfg.voice ? 1 : 0, Volume:cfg.volume});
    },
    cancel: cancel,
    handle: function (payload) {
      if (!payload) return;
      if (payload.RequestType === 'ready') return this.sync();
      if (payload.RequestType === 'cancel') {
        if (active && payload.RequestId === active.id) cancel();
        return;
      }
      if (payload.AudioAck !== undefined) {
        var a = active;
        if (!a || payload.RequestId !== a.id || !a.waiting || payload.AudioAck !== a.waiting.AudioSequence) return;
        var packet = a.waiting;
        if (a.timer) clear(a.timer);
        a.waiting = null;
        if (packet.AudioEnd) return;
        if (packet.AudioChunk) a.offset = a.pendingEnd;
        a.sequence++;
        return next(a);
      }
      if (['inspect','demo','replay'].indexOf(payload.RequestType) === -1 ||
          typeof payload.RequestId !== 'number' || payload.RequestId < 1 || payload.RequestId > 2147483647 ||
          Math.floor(payload.RequestId) !== payload.RequestId) return;
      begin(payload);
    }
  };
}
module.exports = {createClient:createClient, normalizeSettings:normalizeSettings, decodeAudio:decodeAudio,
  MAX_AUDIO:MAX_AUDIO, CHUNK:CHUNK, DEFAULT_ENDPOINT:DEFAULT_ENDPOINT};
