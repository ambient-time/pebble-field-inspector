'use strict';
var protocol = require('./protocol');
var queue = [], sending = false;
function flush() {
  if (sending || !queue.length) return;
  sending = true;
  var job = queue.shift();
  if (job.valid && !job.valid()) { sending = false; flush(); return; }
  Pebble.sendAppMessage(job.packet, function () {
    sending = false; job.done(); flush();
  }, function () {
    sending = false; job.failed(); flush();
  });
}
var client = protocol.createClient({
  send:function (packet, done, failed, valid) { queue.push({packet:packet, done:done, failed:failed, valid:valid}); flush(); },
  request:function (method, endpoint, body, done) {
    var xhr, finished = false;
    function finish(err, data) { if (!finished) { finished = true; done(err, data); } }
    function stop() { finished = true; if (xhr) { try { xhr.abort(); } catch (_) {} } }
    // The matching companion intercepts this reserved address; no relay fallback.
    if (endpoint.indexOf(protocol.BASE) !== 0) { finish('Invalid native bridge address.'); return stop; }
    try {
      xhr = new XMLHttpRequest(); xhr.open(method, endpoint, true); xhr.timeout = 10000;
      xhr.setRequestHeader('Content-Type', 'application/json');
      xhr.onload = function () {
        if (xhr.responseText.length > 65536) return finish('Companion response is too large.');
        if (xhr.status < 200 || xhr.status >= 300) return finish('Check Signal Station settings in the lab companion.');
        var value; try { value = xhr.responseText ? JSON.parse(xhr.responseText) : {}; }
        catch (_) { return finish('Invalid companion response.'); }
        finish(null, value);
      };
      xhr.onerror = xhr.ontimeout = function () { finish('Open Signal Station in the matching lab companion.'); };
      xhr.send(body ? JSON.stringify(body) : null);
    } catch (_) { finish('Open Signal Station in the matching lab companion.'); }
    return stop;
  }
});
Pebble.addEventListener('ready', function () { client.sync(); });
Pebble.addEventListener('appmessage', function (event) { client.handle(event && event.payload); });
Pebble.addEventListener('showConfiguration', function () { client.settings(); });
Pebble.addEventListener('configmessage', function (event) {
  if (!event || !event.data) return;
  try {
    client.configuration(typeof event.data === 'string' ? JSON.parse(event.data) : event.data);
    if (event.respond) event.respond({accepted:true});
  } catch (_) { if (event.respond) event.respond({accepted:false}); }
});
