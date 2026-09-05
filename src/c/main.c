// Field Inspector — Luke Steuber. Button-led dictation and readable replies.
#include <pebble.h>
#include "audio_buffer.h"
#define PERSIST_REQUEST_ID 1
#define PERSIST_CONFIGURED 2
#define PERSIST_VOICE 3
#define PERSIST_VOLUME 4
#define TEXT_CAP 1024

typedef enum { STATE_IDLE, STATE_DICTATING, STATE_WAITING, STATE_LOADING, STATE_SPEAKING, STATE_READY, STATE_ERROR } State;
static Window *s_window;
static Layer *s_canvas;
static Layer *s_body_clip;
static void update_body_layout(void);
static State s_state = STATE_IDLE;
static bool s_configured, s_voice = true, s_help, s_demo;
static uint8_t s_volume = 65;
static uint32_t s_request_id;
static char s_answer[TEXT_CAP], s_status[128], s_notice_text[TEXT_CAP + 160];
static bool s_notice;
static int16_t s_scroll, s_scroll_max;
static AppTimer *s_timeout;
static AppTimer *s_request_timer;
static bool s_request_pending;
static char s_request_kind[12], s_prompt[401];
static void dispatch_request(void *data);
static uint32_t s_completed_sequence;
static bool s_ack_pending;
static uint32_t s_ack_sequence;
#ifdef PBL_MICROPHONE
static DictationSession *s_dictation;
#endif
#ifdef PBL_SPEAKER
static FiAudio s_audio;
static uint8_t s_pending[FI_AUDIO_CHUNK];
static uint16_t s_pending_size;
static uint32_t s_pending_sequence;
static AppTimer *s_pump;
static bool s_stream_started, s_stream_draining;
#endif

static void redraw(void) { if (s_canvas) { update_body_layout(); layer_mark_dirty(s_canvas); if (s_body_clip) layer_mark_dirty(s_body_clip); } }
static bool busy(void) { return s_state == STATE_DICTATING || s_state == STATE_WAITING || s_state == STATE_LOADING || s_state == STATE_SPEAKING; }
static bool speaker_available(void) {
#ifdef PBL_SPEAKER
  return true;
#else
  return false;
#endif
}
static bool speaker_muted(void) {
#ifdef PBL_SPEAKER
  return speaker_is_muted();
#else
  return true;
#endif
}
static void clear_timeout(void) { if (s_timeout) { app_timer_cancel(s_timeout); s_timeout = NULL; } }
static void stop_audio(void) {
#ifdef PBL_SPEAKER
  if (s_pump) { app_timer_cancel(s_pump); s_pump = NULL; }
  // Update state before stopping: finished callbacks must not revive this turn.
  s_stream_started = s_stream_draining = false;
  s_pending_size = 0;
  memset(&s_audio, 0, sizeof s_audio);
  speaker_stop();
#endif
  s_ack_pending = false;
}
static void send_cancel(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_cstring(iter, MESSAGE_KEY_RequestType, "cancel");
  dict_write_uint32(iter, MESSAGE_KEY_RequestId, s_request_id);
  app_message_outbox_send();
}
static void cancel_turn(const char *status) {
  bool was_busy = busy();
#ifdef PBL_SPEAKER
  if (was_busy && s_audio.total) APP_LOG(APP_LOG_LEVEL_INFO, "FieldInspector: stopped request %lu bytes=%lu", (unsigned long)s_request_id, (unsigned long)s_audio.received);
#endif
  if (s_request_timer) { app_timer_cancel(s_request_timer); s_request_timer = NULL; }
  s_request_pending = false;
  s_state = s_answer[0] ? STATE_READY : STATE_IDLE;
#ifdef PBL_MICROPHONE
  if (s_dictation) dictation_session_stop(s_dictation);
#endif
  clear_timeout(); stop_audio(); if (was_busy) send_cancel();
  snprintf(s_status, sizeof s_status, "%s", status);
  s_notice = s_answer[0] && status[0];
  if (s_notice) s_scroll = 0;
  redraw();
}
static void fail(const char *message) {
  s_notice = true;
  if (s_request_timer) { app_timer_cancel(s_request_timer); s_request_timer = NULL; }
  s_request_pending = false;
  s_state = STATE_ERROR;
  s_scroll = 0;
  clear_timeout(); stop_audio(); send_cancel();
  snprintf(s_status, sizeof s_status, "%s", message);
  redraw();
}
static void timeout(void *data) { s_timeout = NULL; fail("Connection timed out. Select to retry."); }
static void arm_timeout(uint32_t ms) { clear_timeout(); s_timeout = app_timer_register(ms, timeout, NULL); }
static void next_request(void) {
  if (s_request_id >= 2147483646) s_request_id = 0;
  s_request_id++;
  persist_write_int(PERSIST_REQUEST_ID, (int32_t)s_request_id);
  s_completed_sequence = 0;
  s_demo = false; s_notice = false; s_scroll = 0; s_status[0] = '\0';
}
static void dispatch_request(void *data) {
  s_request_timer = NULL;
  if (!s_request_pending || s_state != STATE_WAITING) return;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    s_request_timer = app_timer_register(100, dispatch_request, NULL); return;
  }
  dict_write_cstring(iter, MESSAGE_KEY_RequestType, s_request_kind);
  dict_write_uint32(iter, MESSAGE_KEY_RequestId, s_request_id);
  dict_write_uint8(iter, MESSAGE_KEY_SpeakerAvailable, speaker_available());
  dict_write_uint8(iter, MESSAGE_KEY_Muted, speaker_muted());
  if (s_prompt[0]) dict_write_cstring(iter, MESSAGE_KEY_Prompt, s_prompt);
  if (app_message_outbox_send() == APP_MSG_OK) s_request_pending = false;
  else s_request_timer = app_timer_register(100, dispatch_request, NULL);
}
static void send_request(const char *kind, const char *prompt) {
  snprintf(s_request_kind, sizeof s_request_kind, "%s", kind);
  snprintf(s_prompt, sizeof s_prompt, "%s", prompt ? prompt : "");
  s_state = STATE_WAITING;
  APP_LOG(APP_LOG_LEVEL_INFO, "FieldInspector: request %lu %s", (unsigned long)s_request_id, kind);
  snprintf(s_status, sizeof s_status, "%s", strcmp(kind, "demo") == 0 ? "Loading offline demo..." : "Sending through phone...");
  s_request_pending = true;
  arm_timeout(115000);
  dispatch_request(NULL);
  redraw();
}
static void send_ack(uint32_t sequence) {
  s_ack_pending = true; s_ack_sequence = sequence;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_uint32(iter, MESSAGE_KEY_RequestId, s_request_id);
  dict_write_uint32(iter, MESSAGE_KEY_AudioAck, sequence);
  if (app_message_outbox_send() == APP_MSG_OK) s_ack_pending = false;
}

#ifdef PBL_SPEAKER
static void audio_finished(SpeakerFinishReason reason, void *context) {
  if (!s_stream_started) return;
  APP_LOG(APP_LOG_LEVEL_INFO, "FieldInspector: audio finished reason=%d bytes=%lu", (int)reason, (unsigned long)s_audio.received);
  bool drained = s_stream_draining;
  s_stream_started = s_stream_draining = false;
  if (reason == SpeakerFinishReasonDone && drained) {
    clear_timeout(); s_state = STATE_READY;
    snprintf(s_status, sizeof s_status, "%s", s_demo ? "Demo tone finished" : "Reply finished");
    redraw();
  } else if (reason != SpeakerFinishReasonStopped) {
    fail("Voice interrupted. The text is still here.");
  }
}
static void pump(void *data) {
  s_pump = NULL;
  if (!s_audio.total || s_stream_draining) return;
  if (speaker_is_muted() || !s_voice) { cancel_turn("Voice muted. The text is still here."); return; }
  if (s_pending_size) {
    FiAudioResult accepted = fi_audio_accept(&s_audio, s_pending_sequence, s_pending, s_pending_size);
    if (accepted == FI_AUDIO_OK || accepted == FI_AUDIO_DUPLICATE) {
      s_pending_size = 0; send_ack(s_pending_sequence);
    } else if (accepted != FI_AUDIO_FULL) { fail("Voice packet was invalid. Please retry."); return; }
  }
  if (!s_stream_started && (s_audio.count >= 4096 || s_audio.ended)) {
    if (!speaker_stream_open(SpeakerPcmFormat_8kHz_8bit, s_volume)) { fail("Speaker unavailable. The text is still here."); return; }
    s_stream_started = true; s_state = STATE_SPEAKING; redraw();
  }
  if (s_stream_started && s_audio.count) {
    uint16_t size = fi_audio_contiguous(&s_audio);
    if (size > 512) size = 512;
    uint32_t written = speaker_stream_write(s_audio.bytes + s_audio.read, size);
    if (written > size || !fi_audio_consume(&s_audio, written)) { fail("Speaker error. The text is still here."); return; }
  }
  if (s_stream_started && !s_audio.count && s_audio.ended) {
    s_stream_draining = true;
    speaker_stream_close();
    return;
  }
  if (s_ack_pending) send_ack(s_ack_sequence);
  s_pump = app_timer_register(20, pump, NULL);
}
static void ensure_pump(void) { if (!s_pump) s_pump = app_timer_register(20, pump, NULL); }
#endif

static bool read_uint(Tuple *tuple, uint32_t *out) {
  if (!tuple || (tuple->type != TUPLE_UINT && tuple->type != TUPLE_INT)) return false;
  switch (tuple->length) {
    case 1: *out = tuple->value->uint8; return true;
    case 2: *out = tuple->value->uint16; return true;
    case 4: *out = tuple->value->uint32; return true;
    default: return false;
  }
}
static void inbox(DictionaryIterator *iter, void *context) {
  uint32_t value, id;
#ifdef PBL_SPEAKER
  uint32_t sequence;
#endif
  bool settings = false;
  if (read_uint(dict_find(iter, MESSAGE_KEY_Configured), &value)) {
    s_configured = value != 0; persist_write_bool(PERSIST_CONFIGURED, s_configured); settings = true;
  }
  if (read_uint(dict_find(iter, MESSAGE_KEY_VoiceEnabled), &value)) {
    s_voice = value != 0; persist_write_bool(PERSIST_VOICE, s_voice); settings = true;
  }
  if (read_uint(dict_find(iter, MESSAGE_KEY_Volume), &value) && value <= 100) {
    s_volume = value; persist_write_int(PERSIST_VOLUME, value); settings = true;
  }
  if (settings) {
    if (busy()) cancel_turn("Settings saved. Select to ask again.");
    else snprintf(s_status, sizeof s_status, "%s", s_configured ? "Ready to ask" : "Phone setup needed");
    redraw();
  }
  if (!read_uint(dict_find(iter, MESSAGE_KEY_RequestId), &id) || id != s_request_id) return;
  if (!busy()) {
#ifdef PBL_SPEAKER
    if (s_completed_sequence && dict_find(iter, MESSAGE_KEY_AudioEnd) &&
        read_uint(dict_find(iter, MESSAGE_KEY_AudioSequence), &sequence) && sequence == s_completed_sequence) send_ack(sequence);
#endif
    return;
  }
  Tuple *text = dict_find(iter, MESSAGE_KEY_ResponseText);
  Tuple *status = dict_find(iter, MESSAGE_KEY_StatusText);
  if (text && text->type == TUPLE_CSTRING && text->length <= TEXT_CAP) {
    snprintf(s_answer, sizeof s_answer, "%s", text->value->cstring);
    s_scroll = 0;
    if (read_uint(dict_find(iter, MESSAGE_KEY_Demo), &value)) s_demo = value != 0;
    bool audio = read_uint(dict_find(iter, MESSAGE_KEY_AudioExpected), &value) && value;
    s_state = audio ? STATE_LOADING : STATE_READY;
    APP_LOG(APP_LOG_LEVEL_INFO, "FieldInspector: reply %lu bytes=%u demo=%d audio=%d", (unsigned long)id, (unsigned)strlen(s_answer), s_demo, audio);
    if (!audio) clear_timeout(); else arm_timeout(20000);
    if (status && status->type == TUPLE_CSTRING) {
      snprintf(s_status, sizeof s_status, "%s", status->value->cstring);
      s_notice = strcmp(s_status, "Reply ready") != 0 && strcmp(s_status, "OFFLINE DEMO") != 0;
    }
    redraw();
    return;
  }
  if (status && status->type == TUPLE_CSTRING) { fail(status->value->cstring); return; }
#ifdef PBL_SPEAKER
  if (!s_voice || speaker_is_muted()) { cancel_turn("Voice muted. The text is still here."); return; }
  Tuple *begin = dict_find(iter, MESSAGE_KEY_AudioBegin);
  if (begin) {
    if (!read_uint(dict_find(iter, MESSAGE_KEY_AudioSequence), &sequence) || sequence != 0 || !read_uint(begin, &value) || !value || value > FI_AUDIO_LIMIT) { fail("Voice reply is too large."); return; }
    // A resent begin packet must never erase audio already accepted.
    if (s_audio.total) { if (s_audio.total == value) send_ack(0); return; }
    if (!fi_audio_begin(&s_audio, value)) { fail("Invalid voice stream."); return; }
    APP_LOG(APP_LOG_LEVEL_INFO, "FieldInspector: audio begin %lu bytes=%lu", (unsigned long)id, (unsigned long)value);
    arm_timeout(20000); send_ack(0); ensure_pump(); return;
  }
  if (!read_uint(dict_find(iter, MESSAGE_KEY_AudioSequence), &sequence)) return;
  Tuple *chunk = dict_find(iter, MESSAGE_KEY_AudioChunk);
  if (chunk && chunk->type == TUPLE_BYTE_ARRAY) {
    FiAudioResult result = fi_audio_accept(&s_audio, sequence, chunk->value->data, chunk->length);
    if (result == FI_AUDIO_OK || result == FI_AUDIO_DUPLICATE) {
      arm_timeout(20000); send_ack(sequence);
    } else if (result == FI_AUDIO_FULL) {
      if (s_pending_size && s_pending_sequence != sequence) { fail("Voice packet arrived out of order."); return; }
      memcpy(s_pending, chunk->value->data, chunk->length);
      s_pending_size = chunk->length; s_pending_sequence = sequence;
    } else { fail("Voice packet was invalid. Please retry."); return; }
    ensure_pump();
  } else if (dict_find(iter, MESSAGE_KEY_AudioEnd)) {
    FiAudioResult result = fi_audio_end(&s_audio, sequence);
    if (result != FI_AUDIO_OK && result != FI_AUDIO_DUPLICATE) { fail("Voice reply was incomplete. Text is ready."); return; }
    s_completed_sequence = sequence;
    send_ack(sequence); ensure_pump();
  }
#endif
}
static void outbox_sent(DictionaryIterator *iter, void *context) {
  if (s_ack_pending) send_ack(s_ack_sequence);
  else if (s_request_pending && !s_request_timer) dispatch_request(NULL);
}
static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  if (dict_find(iter, MESSAGE_KEY_AudioAck)) {
    uint32_t sequence;
    if (read_uint(dict_find(iter, MESSAGE_KEY_AudioAck), &sequence)) { s_ack_pending = true; s_ack_sequence = sequence; }
  } else {
    uint32_t id;
    if (read_uint(dict_find(iter, MESSAGE_KEY_RequestId), &id) && id == s_request_id && busy()) fail("Phone connection lost. Please try again.");
  }
}
static void inbox_dropped(AppMessageResult reason, void *context) { if (busy()) fail("Watch message was lost. Please retry."); }
static void connection_changed(bool connected) { if (!connected && busy()) fail("Phone disconnected. The last text is kept."); }

#ifdef PBL_MICROPHONE
static void dictation_done(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if (s_state != STATE_DICTATING) return;
  APP_LOG(APP_LOG_LEVEL_INFO, "FieldInspector: dictation status=%d", (int)status);
  if (status == DictationSessionStatusSuccess && transcription && transcription[0]) {
    send_request("inspect", transcription);
  } else if (status == DictationSessionStatusFailureTranscriptionRejected) {
    cancel_turn("Question cancelled. Select to ask.");
  } else if (status == DictationSessionStatusFailureNoSpeechDetected) {
    fail("No speech heard. Select and speak near the watch.");
  } else if (status == DictationSessionStatusFailureConnectivityError) {
    fail("Dictation needs a connected phone and internet.");
  } else if (status == DictationSessionStatusFailureDisabled) {
    fail("Dictation is disabled in the phone app.");
  } else {
    fail("Dictation could not finish. Please try again.");
  }
}
#endif
static void ask(void) {
  if (busy()) { cancel_turn("Stopped. Select to ask a new question."); return; }
  if (!connection_service_peek_pebble_app_connection()) { fail("Connect the Pebble phone app first."); return; }
  if (!s_configured) { s_help = true; snprintf(s_status, sizeof s_status, "Add a token in phone settings."); redraw(); return; }
#ifdef PBL_MICROPHONE
  stop_audio(); next_request();
  if (!s_dictation) {
    s_dictation = dictation_session_create(401, dictation_done, NULL);
    if (s_dictation) { dictation_session_enable_confirmation(s_dictation, true); dictation_session_enable_error_dialogs(s_dictation, false); }
  }
  if (!s_dictation) { fail("Dictation unavailable. Check the phone app."); return; }
  s_state = STATE_DICTATING;
  DictationSessionStatus result = dictation_session_start(s_dictation);
  if (result != DictationSessionStatusSuccess) { fail("Could not start dictation. Check the phone app."); return; }
  redraw();
#else
  fail("This watch has no microphone. Help has an offline demo.");
#endif
}
static void demo(void) {
  cancel_turn(""); s_help = false; s_answer[0] = '\0'; next_request(); s_demo = true;
  send_request("demo", NULL);
}
static void select_click(ClickRecognizerRef r, void *context) { if (s_help) demo(); else ask(); }
static void up_click(ClickRecognizerRef r, void *context) { s_scroll -= 36; if (s_scroll < 0) s_scroll = 0; redraw(); }
static void down_click(ClickRecognizerRef r, void *context) { s_scroll += 36; if (s_scroll > s_scroll_max) s_scroll = s_scroll_max; redraw(); }
static void up_long(ClickRecognizerRef r, void *context) {
  cancel_turn(""); s_help = false; next_request(); send_request("replay", NULL);
}
static void down_long(ClickRecognizerRef r, void *context) {
  cancel_turn(""); s_help = true; s_scroll = 0; redraw();
}
static void back_click(ClickRecognizerRef r, void *context) {
  if (s_help) { s_help = false; s_scroll = 0; redraw(); }
  else if (busy()) cancel_turn("Stopped. Select to ask again.");
  else window_stack_pop(true);
}
static void clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
  window_long_click_subscribe(BUTTON_ID_UP, 650, up_long, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 650, down_long, NULL);
}
static GRect body_bounds(GRect bounds) {
  int w = bounds.size.w, h = bounds.size.h;
  bool round = PBL_IF_ROUND_ELSE(true, false), big = h >= 200;
  int inset = round ? w / 7 : 6, top = round ? h / 10 : 2;
  int footer_y = h - (round ? h / 9 : 2) - (big ? 48 : 36);
  int body_y = top + (big ? 34 : 26);
  return GRect(inset, body_y, w - 2 * inset, footer_y - body_y - 4);
}
static const char *body_text(void) {
  if (s_help) return "Phone setup:\nOpen this app's settings. Add an installation token.\n\nSelect: offline demo\nHold Up: replay reply\nUp/Down: scroll\nBack: stop or leave\n\nSelect to ask. Speak, then confirm. Replies also stay readable when voice is muted or unavailable.";
  if (s_state == STATE_DICTATING) return "Speak near the watch.\n\nReview the transcript, then Select to send. Back cancels.";
  if (s_notice && s_answer[0]) {
    snprintf(s_notice_text, sizeof s_notice_text, "%s\n\n%s", s_status, s_answer);
    return s_notice_text;
  }
  if (s_state == STATE_ERROR) return s_status;
  if (s_state == STATE_WAITING) return "The phone is preparing a short answer.\n\nBack cancels.";
  if (s_answer[0]) return s_answer;
  return s_configured ? "Select: ask\nSpeak, then confirm.\n\nHold Down: help" : "Phone setup needed.\n\nHold Down for Help and an offline demo.";
}
static GFont body_font(void) {
  return fonts_get_system_font(layer_get_bounds(s_canvas).size.h >= 200 ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
}
static void update_body_layout(void) {
  if (!s_body_clip || !s_canvas) return;
  GRect box = body_bounds(layer_get_unobstructed_bounds(s_canvas));
  layer_set_frame(s_body_clip, box);
  GSize size = graphics_text_layout_get_content_size(body_text(), body_font(), GRect(0,0,box.size.w,2000), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  s_scroll_max = size.h > box.size.h ? size.h - box.size.h : 0;
  if (s_scroll > s_scroll_max) s_scroll = s_scroll_max;
}
static void draw_body(Layer *layer, GContext *ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, body_text(), body_font(), GRect(0,-s_scroll,layer_get_bounds(layer).size.w,2000),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}
static void draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(layer);
  int w = bounds.size.w, h = bounds.size.h;
  bool round = PBL_IF_ROUND_ELSE(true, false), big = h >= 200;
  int inset = round ? w / 7 : 6, top = round ? h / 10 : 2;
  int footer_y = h - (round ? h / 9 : 2) - (big ? 48 : 36);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  const char *heading = s_help ? "FIELD MANUAL" : s_demo ? "OFFLINE DEMO" :
    s_state == STATE_DICTATING ? "LISTENING" : s_state == STATE_WAITING ? "CONTACTING" :
    s_state == STATE_LOADING ? "LOADING VOICE" : s_state == STATE_SPEAKING ? "SPEAKING" :
    s_state == STATE_ERROR ? "TRY AGAIN" : s_answer[0] ? "FIELD REPORT" : "FIELD INSPECTOR";
  graphics_draw_text(ctx, heading, fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
    GRect(inset, top, w - 2 * inset, big ? 30 : 22), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  const char *footer = s_help ? "Select: demo tone" : busy() ? "Back: stop" : "Select: ask";
  graphics_draw_text(ctx, footer, fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
    GRect(inset, footer_y, w - 2 * inset, big ? 23 : 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_help || s_scroll_max > 0 ? "Up/Down: read" : "Hold Down: help",
    fonts_get_system_font(big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14),
    GRect(inset, footer_y + (big ? 23 : 18), w - 2 * inset, big ? 23 : 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, draw); layer_add_child(root, s_canvas);
  s_body_clip = layer_create(body_bounds(layer_get_bounds(root)));
  layer_set_update_proc(s_body_clip, draw_body);
  layer_add_child(root, s_body_clip);
  update_body_layout();
}
static void window_unload(Window *window) {
  layer_destroy(s_body_clip); s_body_clip = NULL; layer_destroy(s_canvas); s_canvas = NULL;
}
static void init(void) {
  s_request_id = persist_exists(PERSIST_REQUEST_ID) ? (uint32_t)persist_read_int(PERSIST_REQUEST_ID) : (uint32_t)time(NULL);
  s_configured = persist_exists(PERSIST_CONFIGURED) && persist_read_bool(PERSIST_CONFIGURED);
  if (persist_exists(PERSIST_VOICE)) s_voice = persist_read_bool(PERSIST_VOICE);
  if (persist_exists(PERSIST_VOLUME)) s_volume = persist_read_int(PERSIST_VOLUME);
  s_window = window_create(); window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){.load=window_load,.unload=window_unload});
  window_set_click_config_provider(s_window, clicks);
  app_message_register_inbox_received(inbox);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(2048, 1024);
  connection_service_subscribe((ConnectionHandlers){.pebble_app_connection_handler=connection_changed});
#ifdef PBL_SPEAKER
  speaker_set_finish_callback(audio_finished, NULL);
#endif
  window_stack_push(s_window, true);
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) { dict_write_cstring(iter, MESSAGE_KEY_RequestType, "ready"); app_message_outbox_send(); }
}
static void deinit(void) {
  if (s_request_timer) app_timer_cancel(s_request_timer);
  clear_timeout(); stop_audio(); send_cancel();
#ifdef PBL_MICROPHONE
  if (s_dictation) dictation_session_destroy(s_dictation);
#endif
#ifdef PBL_SPEAKER
  speaker_set_finish_callback(NULL, NULL);
#endif
  connection_service_unsubscribe(); app_message_deregister_callbacks(); window_destroy(s_window);
}
int main(void) { init(); app_event_loop(); deinit(); }
