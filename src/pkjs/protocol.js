'use strict';
// Signal Station native bridge. Provider keys never enter this runtime.
var BASE = 'https://field-inspector.invalid/native/v1/';
var MAX_TEXT_BYTES = 900;
function utf8Bytes(s) {
  try { return unescape(encodeURIComponent(s)).length; } catch (_) { return Infinity; }
}
function requestId(id) { return typeof id === 'number' && id > 0 && id <= 2147483647 && Math.floor(id) === id; }
function createClient(options) {
  var active = null, generation = 0;
  var later = options.setTimer || setTimeout, clear = options.clearTimer || clearTimeout;
  function send(packet, done, failed) {
    var owner = generation;
    options.send(packet, done || function () {}, failed || function () {}, function () { return owner === generation; });
  }
  function native(method, path, body, done) { return options.request(method, BASE + path, body, done || function () {}); }
  function valid(a) { return active === a && a.generation === generation; }
  function cancel(fromNative) {
    var old = active;
    generation++; active = null;
    if (old) {
      if (old.timer) clear(old.timer);
      if (old.abort) old.abort();
      // XHR abort alone does not cancel a native coroutine.
      if (!fromNative) native('POST', 'cancel', {request_id:old.id});
    }
  }
  function error(a, message) {
    if (!valid(a)) return;
    if (a.timer) clear(a.timer);
    a.terminal = true;
    send({RequestId:a.id, StatusText:message || 'Open Signal Station in the lab companion.', Complete:1});
  }
  function deliver(a) {
    if (!valid(a) || a.delivered) return;
    if (++a.deliveries > 3) return error(a, 'Watch delivery timed out. Ask again when connected.');
    send({RequestId:a.id, ResponseText:a.text, Complete:1}, function () {
      if (valid(a) && !a.delivered) a.timer = later(function () { deliver(a); }, 3000);
    }, function () { error(a, 'Watch connection lost.'); });
  }
  function acknowledge(a, attempt) {
    if (!valid(a)) return;
    a.committing = true;
    native('POST', 'delivered', {request_id:a.id}, function (err) {
      if (!valid(a)) return;
      if (!err) { a.delivered = true; a.committing = false; return; }
      if (attempt < 3) a.timer = later(function () { acknowledge(a, attempt + 1); }, 500);
      else { a.committing = false; error(a, 'Reply received; session acknowledgement failed.'); }
    });
  }
  function drainWatchData(a) {
    if (!valid(a) || a.uploading || !a.watchQueue.length) return;
    a.uploading = true;
    var packet = a.watchQueue.shift();
    native('POST', 'watch-data', packet, function (err) {
      if (!valid(a)) return;
      a.uploading = false;
      if (err) return error(a, err);
      drainWatchData(a);
    });
  }
  function poll(a) {
    if (!valid(a)) return;
    if (++a.polls > 180) return error(a, 'Request timed out. Back stops this request.');
    a.abort = native('GET', 'status?request_id=' + a.id, null, function (err, data) {
      if (!valid(a)) return;
      if (err || !data) return error(a, err);
      if (data.state === 'error') return error(a, data.status || 'Request failed. Check the phone.');
      if (data.state === 'ready') {
        if (typeof data.text !== 'string' || !data.text.trim() || utf8Bytes(data.text) > MAX_TEXT_BYTES) {
          return error(a, 'Invalid report size. Check the phone.');
        }
        a.text = data.text; a.terminal = true; a.deliveries = 0;
        return deliver(a);
      }
      if (data.state !== 'working') return error(a, 'Unknown companion response.');
      if (data.status && data.status !== a.lastStatus) {
        a.lastStatus = data.status;
        send({RequestId:a.id, StatusText:String(data.status).slice(0,90), Complete:0});
      }
      a.timer = later(function () { poll(a); }, 500);
    });
  }
  function attach(id) {
    if (active && active.id === id) return active;
    cancel();
    active = {id:id, generation:generation, timer:null, abort:null, polls:0, terminal:false, delivered:false, watchQueue:[]};
    return active;
  }
  function sync() {
    native('GET', 'capabilities', null, function (err, cfg) {
      if (err || !cfg) return send({Configured:0, StatusText:'Open Signal Station in the lab companion.'});
      send({Configured:cfg.configured ? 1 : 0, Enabled:JSON.stringify(cfg.enabled || []),
        ConfirmTranscript:cfg.confirmTranscript ? 1 : 0, ReducedMotion:cfg.reducedMotion ? 1 : 0});
    });
  }
  return {
    sync:sync,
    cancel:cancel,
    configuration:function (command) {
      if (command && command.kind === 'cancel' && requestId(command.request_id)) {
        if (active && active.id === command.request_id) {
          cancel(true);
          send({RequestId:command.request_id, Command:'cancel'});
        }
        return;
      }
      if (!command || ['survey','record','ask'].indexOf(command.kind) < 0 || !requestId(command.request_id)) return;
      var already = active && active.id === command.request_id;
      var a = attach(command.request_id);
      if (command.kind === 'record') a.recording = true;
      send({RequestId:a.id, Command:command.kind, Enabled:JSON.stringify(command.enabled || []),
        ConfirmTranscript:command.confirmTranscript ? 1 : 0});
      if (!already && command.kind !== 'record') poll(a);
    },
    settings:function () { native('POST', 'settings', {}); },
    handle:function (p) {
      if (!p) return;
      if (p.RequestType === 'ready') return sync();
      if (p.RequestType === 'clear') { cancel(); return native('POST', 'clear', {}, function () { sync(); }); }
      if (p.RequestType === 'settings') return native('POST', 'settings', {});
      if (!requestId(p.RequestId)) return;
      if (p.RequestType === 'cancel') { if (active && active.id === p.RequestId) cancel(); return; }
      if (p.TextAck !== undefined) {
        var a = active;
        if (!a || a.id !== p.RequestId || !a.text || a.delivered || a.committing) return;
        if (a.timer) clear(a.timer);
        return acknowledge(a, 1);
      }
      if (p.Snapshot !== undefined || p.RequestType === 'watch-data') {
        if (!active || active.id !== p.RequestId || active.terminal) return;
        var observations;
        try { observations = p.Snapshot ? JSON.parse(p.Snapshot) : []; } catch (_) { return error(active, 'Invalid watch readings.'); }
        if (!Array.isArray(observations) || observations.length > 12) return error(active, 'Invalid watch readings.');
        if (active.watchQueue.length >= 24) return error(active, 'Too many watch packets.');
        active.watchQueue.push({request_id:p.RequestId, observations:observations, complete:!!p.Complete});
        return drainWatchData(active);
      }
      if (['ask','survey','record'].indexOf(p.RequestType) < 0) return;
      var recordTransition = active && active.id === p.RequestId && active.recording && p.RequestType === 'ask';
      if (active && active.id === p.RequestId && !recordTransition) return; // A transport retry must not bill twice.
      if (p.RequestType === 'ask' && (typeof p.Prompt !== 'string' || !p.Prompt.trim() || utf8Bytes(p.Prompt) > 400)) {
        return send({RequestId:p.RequestId, StatusText:'Question is empty or too long.', Complete:1});
      }
      var a = attach(p.RequestId);
      a.recording = false;
      a.abort = native('POST', 'start', {kind:p.RequestType, request_id:p.RequestId, prompt:p.Prompt || undefined}, function (err) {
        if (!valid(a)) return;
        if (err) return error(a, err);
        poll(a);
      });
    }
  };
}
module.exports = {createClient:createClient, BASE:BASE, utf8Bytes:utf8Bytes, MAX_TEXT_BYTES:MAX_TEXT_BYTES};
