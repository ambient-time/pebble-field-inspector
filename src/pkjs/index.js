'use strict';
var Clay = require('./vendor/pebble-clay');
var protocol = require('./protocol');
var clay = new Clay(require('./config.json'), null, {autoHandleEvents:false});
var queue = [], sending = false;

function flush() {
  if (sending || !queue.length) return;
  sending = true;
  var job = queue.shift();
  Pebble.sendAppMessage(job.packet, function () {
    sending = false;
    job.done();
    flush();
  }, function () {
    sending = false;
    job.failed();
    flush();
  });
}
function loadSettings() {
  try { return JSON.parse(localStorage.getItem('clay-settings') || '{}'); }
  catch (e) { return {}; }
}
var client = protocol.createClient({
  settings:loadSettings,
  send:function (packet, done, failed) { queue.push({packet:packet, done:done, failed:failed}); flush(); },
  fetchJson:function (endpoint, token, body, done) {
    var xhr = new XMLHttpRequest(), finished = false;
    function finish(error, data) { if (finished) return; finished = true; done(error, data); }
    xhr.open('POST', endpoint, true);
    xhr.timeout = 105000;
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('Authorization', 'Bearer ' + token);
    xhr.onload = function () {
      if (xhr.responseText.length > 190000) return finish('Service reply is too large. Please retry.');
      var data;
      try { data = JSON.parse(xhr.responseText); } catch (e) { return finish('Invalid service reply. Please retry.'); }
      if (xhr.status === 401 || xhr.status === 403) return finish('Installation token rejected. Open phone settings.');
      if (xhr.status === 429) return finish('Too many requests. Wait a moment, then retry.');
      if (xhr.status < 200 || xhr.status >= 300) return finish('Service unavailable. Please try again later.');
      finish(null, data);
    };
    xhr.onerror = function () { finish('Phone has no service connection. Try again.'); };
    xhr.ontimeout = function () { finish('Service timed out. Please try again.'); };
    xhr.send(JSON.stringify(body));
    return function () { finished = true; xhr.abort(); };
  }
});
Pebble.addEventListener('ready', function () { client.sync(); });
Pebble.addEventListener('appmessage', function (event) { client.handle(event && event.payload); });
Pebble.addEventListener('showConfiguration', function () { Pebble.openURL(clay.generateUrl()); });
Pebble.addEventListener('webviewclosed', function (event) {
  if (!event || !event.response || event.response === 'CANCELLED') return;
  try {
    // false disables AppMessage conversion: endpoint and token stay phone-only.
    clay.getSettings(event.response, false);
    client.cancel();
    client.sync();
  } catch (e) {
    // Do not log the configuration URL or values; they include a private token.
    console.log('Field Inspector: settings could not be read.');
  }
});
