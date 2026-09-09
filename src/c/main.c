// Signal Station — Luke Steuber. Speak, survey, and read.
#include <pebble.h>
#include "signal_math.h"
#define TEXT_CAP 1024
#define SNAPSHOT_CAP 1900
#define PERSIST_REQUEST_ID 1
typedef enum { VIEW_MENU, VIEW_READER, VIEW_HELP, VIEW_WAIT, VIEW_DICTATION, VIEW_HISTORY, VIEW_REVIEW } View;
static Window *s_window;
static Layer *s_canvas, *s_body;
static View s_view;
static char s_answer[TEXT_CAP], s_history[TEXT_CAP], s_status[160], s_display[TEXT_CAP+200], s_prompt[401], s_review_context[351];
static char s_enabled[900], s_snapshot[SNAPSHOT_CAP], s_kind[16];
static bool s_configured, s_bridge_ready, s_confirm, s_connected, s_collecting, s_sampling, s_phone_record;
static int s_scroll, s_scroll_max, s_stage;
static uint32_t s_request_id, s_answer_id, s_cancel_id, s_ack_id;
static uint32_t s_collection_id;
static bool s_request_pending, s_ready_pending, s_clear_pending, s_settings_pending, s_snapshot_pending;
static bool s_outbox_busy, s_snapshot_complete;
static int s_outbox_kind, s_retries;
static uint32_t s_outbox_id;
static AppTimer *s_outbox_timer, *s_timeout_timer, *s_sample_timer;
static SignalMotion s_motion;
static CompassHeadingData s_compass;
static time_t s_collected_at;
#ifdef PBL_MICROPHONE
static DictationSession *s_dictation;
#endif
static const char *s_items[] = {"Capture", "Ask", "History"};
static void redraw(void);
static void flush(void *unused);
static void next_snapshot(void);
static void ask(void);
static bool busy(void) { return s_view == VIEW_WAIT || s_view == VIEW_DICTATION || s_view == VIEW_REVIEW; }
static bool enabled(const char *key) {
  char quoted[64]; snprintf(quoted, sizeof quoted, "\"%s\"", key);
  return strstr(s_enabled, quoted) != NULL;
}
static void stop_sampling(void) {
  if (s_sample_timer) { app_timer_cancel(s_sample_timer); s_sample_timer = NULL; }
  if (s_sampling) { accel_data_service_unsubscribe(); compass_service_unsubscribe(); s_sampling = false; }
}
static void clear_timeout(void) { if (s_timeout_timer) { app_timer_cancel(s_timeout_timer); s_timeout_timer = NULL; } }
static void cancel_turn(const char *message) {
  if (s_view == VIEW_WAIT || s_view == VIEW_REVIEW || s_collecting || (s_view == VIEW_DICTATION && s_phone_record)) s_cancel_id = s_request_id;
  s_request_pending = s_snapshot_pending = s_collecting = false;
  s_phone_record=false;
  s_view = VIEW_READER;
  stop_sampling(); clear_timeout();
#ifdef PBL_MICROPHONE
  if (s_dictation) dictation_session_stop(s_dictation);
#endif
  snprintf(s_status, sizeof s_status, "%s", message);
  s_scroll = 0; flush(NULL); redraw();
}
static void timeout(void *unused) { s_timeout_timer = NULL; cancel_turn("Timed out. Check the phone, then ask again."); }
static void start_timeout(void) { clear_timeout(); s_timeout_timer = app_timer_register(100000, timeout, NULL); }
static void next_request(void) {
  s_request_id = s_request_id >= 2147483646 ? 1 : s_request_id + 1;
  persist_write_int(PERSIST_REQUEST_ID, s_request_id);
}
static void request(const char *kind, const char *prompt) {
  if (!s_phone_record) next_request();
  s_phone_record=false; s_view = VIEW_WAIT; s_scroll = 0;
  snprintf(s_kind, sizeof s_kind, "%s", kind);
  signal_utf8_copy(s_prompt, sizeof s_prompt, prompt ? prompt : "");
  snprintf(s_status, sizeof s_status, "%s", !strcmp(kind,"history") ? "Loading saved history..." : !strcmp(kind,"capture") ? "Saving selected readings..." : !strcmp(kind,"survey") ? "Surveying selected sources..." : "Asking through the phone...");
  s_request_pending = true; start_timeout(); flush(NULL); redraw();
}
static void retry_flush(void) { if (!s_outbox_timer) s_outbox_timer = app_timer_register(100, flush, NULL); }
// One in-flight message. Cancel and text acknowledgement always take priority.
static void flush(void *unused) {
  s_outbox_timer = NULL;
  if (s_outbox_busy) return;
  int kind = s_cancel_id ? 1 : s_ack_id ? 2 : s_clear_pending ? 3 : s_settings_pending ? 4 :
    s_ready_pending ? 5 : s_request_pending ? 6 : s_snapshot_pending ? 7 : 0;
  if (!kind) return;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) { retry_flush(); return; }
  s_outbox_kind = kind;
  s_outbox_id=s_request_id;
  if (kind == 1) { dict_write_cstring(iter,MESSAGE_KEY_RequestType,"cancel"); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_cancel_id); }
  if (kind == 2) { dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_ack_id); dict_write_uint8(iter,MESSAGE_KEY_TextAck,1); }
  if (kind >= 3 && kind <= 5) dict_write_cstring(iter,MESSAGE_KEY_RequestType,kind==3 ? "clear" : kind==4 ? "settings" : "ready");
  if (kind == 6) {
    dict_write_cstring(iter,MESSAGE_KEY_RequestType,s_kind); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_request_id);
    if (s_prompt[0]) dict_write_cstring(iter,MESSAGE_KEY_Prompt,s_prompt);
  }
  if (kind == 7) {
    dict_write_cstring(iter,MESSAGE_KEY_RequestType,"watch-data"); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_request_id);
    dict_write_cstring(iter,MESSAGE_KEY_Snapshot,s_snapshot); dict_write_uint8(iter,MESSAGE_KEY_Complete,s_snapshot_complete);
  }
  if (app_message_outbox_send() == APP_MSG_OK) s_outbox_busy = true;
  else retry_flush();
}
static void outbox_sent(DictionaryIterator *iter, void *context) {
  s_outbox_busy = false; s_retries = 0;
  if (s_outbox_kind == 1) { Tuple *t = dict_find(iter,MESSAGE_KEY_RequestId); if (t && t->value->uint32==s_cancel_id) s_cancel_id=0; }
  if (s_outbox_kind == 2) { Tuple *t = dict_find(iter,MESSAGE_KEY_RequestId); if (t && t->value->uint32==s_ack_id) s_ack_id=0; }
  if (s_outbox_kind == 3) s_clear_pending=false;
  if (s_outbox_kind == 4) s_settings_pending=false;
  if (s_outbox_kind == 5) s_ready_pending=false;
  if (s_outbox_kind == 6 && s_outbox_id==s_request_id) s_request_pending=false;
  if (s_outbox_kind == 7 && s_outbox_id==s_request_id) { s_snapshot_pending=false; if (s_collecting) next_snapshot(); }
  flush(NULL);
}
static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  s_outbox_busy=false;
  if (s_outbox_kind>=6 && s_outbox_id!=s_request_id) { s_retries=0; flush(NULL); return; }
  if (++s_retries <= 3) { retry_flush(); return; }
  s_retries=0; s_request_pending=s_snapshot_pending=s_collecting=false;
  // Keep cancellation/acknowledgement pending for reconnect, but do not spin.
  s_ready_pending=s_clear_pending=s_settings_pending=false;
  stop_sampling(); clear_timeout(); s_view=VIEW_READER;
  snprintf(s_status,sizeof s_status,"Phone disconnected. Reconnect, then try again."); redraw();
}
static void inbox_dropped(AppMessageResult reason, void *context) { cancel_turn("Incomplete phone message. Try again."); }
static void accel(AccelData *samples, uint32_t count) {
  for (uint32_t i=0; i<count; i++) signal_motion_add(&s_motion,samples[i].x,samples[i].y,samples[i].z,samples[i].did_vibrate);
}
static void compass(CompassHeadingData data) { s_compass=data; }
static void append_observation(const char *key, const char *value, const char *unit, const char *status,
                               const char *period, time_t from, time_t end, bool date) {
  if (!enabled(key)) return;
  size_t n=strlen(s_snapshot); char date_text[40]="", time_fields[120]="";
  if (date) { char day[16]; strftime(day,sizeof day,"%Y-%m-%d",localtime(&from)); snprintf(date_text,sizeof date_text,",\"date\":\"%s\"",day); }
  if (strcmp(status,"timestamp_unknown")) snprintf(time_fields,sizeof time_fields,",\"measuredAt\":%ld000,\"windowStart\":%ld000,\"windowEnd\":%ld000",(long)end,(long)from,(long)end);
  snprintf(s_snapshot+n,sizeof s_snapshot-n,
    "%s{\"key\":\"%s\",\"source\":\"watch\",\"value\":%s,\"unit\":\"%s\",\"collectedAt\":%ld000,\"status\":\"%s\",\"period\":\"%s\"%s%s}",
    n>1 ? "," : "",key,value,unit,(long)s_collected_at,status,period,time_fields,date_text);
}
#ifdef PBL_HEALTH
static const char *access_status(HealthServiceAccessibilityMask mask) {
  if (mask & HealthServiceAccessibilityMaskNoPermission) return "permission_denied";
  return mask & HealthServiceAccessibilityMaskAvailable ? "fresh" : "unavailable";
}
static void health_metric(const char *key, HealthMetric metric, const char *unit, const char *period, time_t start, time_t end) {
  if (!enabled(key)) return;
  HealthServiceAccessibilityMask mask=health_service_metric_accessible(metric,start,end);
  char value[24]="null";
  if (mask & HealthServiceAccessibilityMaskAvailable) snprintf(value,sizeof value,"%ld",(long)health_service_sum(metric,start,end));
  append_observation(key,value,unit,access_status(mask),period,start,end,true);
}
static time_t s_sleep_start,s_sleep_end;
static bool sleep_episode(HealthActivity activity,time_t start,time_t end,void *context) {
  // Latest completed >=2h episode is a documented heuristic, not a diagnosis.
  if (activity==HealthActivitySleep && end<s_collected_at && end-start>=7200) { s_sleep_start=start; s_sleep_end=end; return false; }
  return true;
}
#endif
static time_t day_start(int ago) {
  struct tm day=*localtime(&s_collected_at); day.tm_hour=day.tm_min=day.tm_sec=0; day.tm_mday-=ago; day.tm_isdst=-1;
  return mktime(&day); // Local calendar days preserve daylight-saving boundaries.
}
static void next_snapshot(void) {
  if (!s_collecting || s_snapshot_pending) return;
  strcpy(s_snapshot,"["); char value[180];
  int stage=s_stage++;
  if (stage==0) {
    if (enabled("watch.battery")) {
      BatteryChargeState battery=battery_state_service_peek();
      snprintf(value,sizeof value,"{\"percent\":%u,\"charging\":%s}",battery.charge_percent,battery.is_charging?"true":"false");
      append_observation("watch.battery",value,"percent","fresh","current",s_collected_at,s_collected_at,false);
    }
  } else if (stage>=1 && stage<=16) {
    // Split each local day across two bounded packets, with no raw minute samples.
    int ago=(stage-1)/2; bool second=(stage-1)%2;
    time_t start=day_start(ago),end=ago ? day_start(ago-1) : s_collected_at;
    const char *period=ago ? "day" : "today";
#ifdef PBL_HEALTH
    if (!second) {
      health_metric("health.steps",HealthMetricStepCount,"steps",period,start,end);
      health_metric("health.active_seconds",HealthMetricActiveSeconds,"seconds",period,start,end);
      health_metric("health.distance",HealthMetricWalkedDistanceMeters,"meters",period,start,end);
      health_metric("health.active_calories",HealthMetricActiveKCalories,"kcal",period,start,end);
    } else {
      health_metric("health.resting_calories",HealthMetricRestingKCalories,"kcal",period,start,end);
      health_metric("health.sleep",HealthMetricSleepSeconds,"seconds",period,start,end);
      health_metric("health.restful_sleep",HealthMetricSleepRestfulSeconds,"seconds",period,start,end);
    }
#else
    const char *keys[]={"health.steps","health.active_seconds","health.distance","health.active_calories","health.resting_calories","health.sleep","health.restful_sleep"};
    for (int i=second?4:0;i<(second?7:4);i++) append_observation(keys[i],"null","","unavailable",period,start,end,true);
#endif
  } else if (stage==17) {
#ifdef PBL_HEALTH
    if (enabled("health.heart_rate")) {
      HealthServiceAccessibilityMask mask=health_service_metric_accessible(HealthMetricHeartRateBPM,s_collected_at-60,s_collected_at);
      HealthValue bpm=health_service_peek_current_value(HealthMetricHeartRateBPM);
      snprintf(value,sizeof value,"%ld",(long)bpm);
      append_observation("health.heart_rate",bpm>0 && (mask & HealthServiceAccessibilityMaskAvailable) ? value : "null","bpm",bpm>0 && (mask & HealthServiceAccessibilityMaskAvailable) ? "timestamp_unknown" : (mask & HealthServiceAccessibilityMaskNoPermission) ? "permission_denied" : "unavailable","current",s_collected_at,s_collected_at,false);
    }
    if (enabled("health.activity")) {
      HealthServiceAccessibilityMask mask=health_service_any_activity_accessible(HealthActivityMaskAll,s_collected_at-60,s_collected_at);
      snprintf(value,sizeof value,"%lu",(unsigned long)health_service_peek_current_activities());
      append_observation("health.activity",mask & HealthServiceAccessibilityMaskAvailable ? value : "null","activity_bitmask",access_status(mask),"current",s_collected_at,s_collected_at,false);
    }
#else
    append_observation("health.heart_rate","null","bpm","unavailable","current",s_collected_at,s_collected_at,false);
    append_observation("health.activity","null","activity_bitmask","unavailable","current",s_collected_at,s_collected_at,false);
#endif
  } else if (stage==18) {
#ifdef PBL_HEALTH
    s_sleep_start=s_sleep_end=0;
    if (enabled("health.sleep") || enabled("health.restful_sleep")) health_service_activities_iterate(HealthActivitySleep,s_collected_at-48*3600,s_collected_at,HealthIterationDirectionPast,sleep_episode,NULL);
    if (s_sleep_end) {
      health_metric("health.sleep",HealthMetricSleepSeconds,"seconds","last_completed_sleep_2h_heuristic",s_sleep_start,s_sleep_end);
      health_metric("health.restful_sleep",HealthMetricSleepRestfulSeconds,"seconds","last_completed_sleep_2h_heuristic",s_sleep_start,s_sleep_end);
    } else {
      append_observation("health.sleep","null","seconds","unavailable","last_completed_sleep_2h_heuristic",s_collected_at-48*3600,s_collected_at,false);
      append_observation("health.restful_sleep","null","seconds","unavailable","last_completed_sleep_2h_heuristic",s_collected_at-48*3600,s_collected_at,false);
    }
#else
    append_observation("health.sleep","null","seconds","unavailable","last_completed_sleep_2h_heuristic",s_collected_at-48*3600,s_collected_at,false);
#endif
  } else if (stage==19) {
    if (s_sampling) { s_stage--; return; }
    snprintf(value,sizeof value,"{\"samples\":%lu,\"mean_x\":%ld,\"mean_y\":%ld,\"mean_z\":%ld,\"peak_abs_axis\":%d}",(unsigned long)s_motion.count,(long)(s_motion.count?s_motion.x/(int)s_motion.count:0),(long)(s_motion.count?s_motion.y/(int)s_motion.count:0),(long)(s_motion.count?s_motion.z/(int)s_motion.count:0),s_motion.peak);
    append_observation("watch.motion",s_motion.count?value:"null","mg",s_motion.count?"fresh":"unavailable","5_second_sample",s_collected_at,s_collected_at+5,false);
    snprintf(value,sizeof value,"%ld",(long)(((TRIG_MAX_ANGLE-s_compass.magnetic_heading)*360LL/TRIG_MAX_ANGLE)%360));
    bool calibrated=s_compass.compass_status==CompassStatusCalibrated;
    append_observation("watch.compass",calibrated?value:"null","degrees_magnetic_clockwise",calibrated?"fresh":s_compass.compass_status==CompassStatusCalibrating?"calibrating":"unavailable","current",s_collected_at,s_collected_at+5,false);
  }
  if (strlen(s_snapshot)+2>=sizeof s_snapshot) { cancel_turn("Watch readings exceeded their limit."); return; }
  strcat(s_snapshot,"]"); s_snapshot_complete=stage>=19;
  if (strlen(s_snapshot)==2 && !s_snapshot_complete) { next_snapshot(); return; }
  s_snapshot_pending=true;
  if (s_snapshot_complete) s_collecting=false;
  flush(NULL);
}
static void sample_done(void *unused) { s_sample_timer=NULL; stop_sampling(); if (s_collecting && s_stage==19) next_snapshot(); }
static void collect(uint32_t id) {
  if (id==s_collection_id) return;
  s_collection_id=id;
  stop_sampling(); s_request_id=id; s_view=VIEW_WAIT; s_collecting=true; s_stage=0;
  s_collected_at=time(NULL); memset(&s_motion,0,sizeof s_motion); s_compass.compass_status=CompassStatusUnavailable;
  if (enabled("watch.motion") || enabled("watch.compass")) {
    s_sampling=true;
    if (enabled("watch.motion")) { accel_data_service_subscribe(10,accel); accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ); }
    if (enabled("watch.compass")) compass_service_subscribe(compass);
    s_sample_timer=app_timer_register(5000,sample_done,NULL);
  }
  snprintf(s_status,sizeof s_status,"Collecting selected watch readings..."); start_timeout(); next_snapshot(); redraw();
}
static void inbox(DictionaryIterator *iter,void *context) {
  Tuple *t=dict_find(iter,MESSAGE_KEY_BridgeReady); if (t) s_bridge_ready=t->value->uint32!=0;
  t=dict_find(iter,MESSAGE_KEY_Configured); if (t) s_configured=t->value->uint32!=0;
  t=dict_find(iter,MESSAGE_KEY_Enabled); if (t && t->type==TUPLE_CSTRING) snprintf(s_enabled,sizeof s_enabled,"%s",t->value->cstring);
  t=dict_find(iter,MESSAGE_KEY_ConfirmTranscript); if (t) s_confirm=t->value->uint32!=0;
  Tuple *id=dict_find(iter,MESSAGE_KEY_RequestId),*command=dict_find(iter,MESSAGE_KEY_Command);
  if (id && command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"review")) {
    if (id->value->uint32==s_request_id) return; // A replay cannot restore a dismissed or confirmed draft.
    Tuple *prompt=dict_find(iter,MESSAGE_KEY_Prompt), *review_context=dict_find(iter,MESSAGE_KEY_ResponseText);
    if (!prompt || prompt->type!=TUPLE_CSTRING || prompt->length<2 || prompt->length>401 ||
        !review_context || review_context->type!=TUPLE_CSTRING || review_context->length<2 || review_context->length>351) return;
    if (busy()) cancel_turn("");
    s_request_id=id->value->uint32; s_view=VIEW_REVIEW; s_scroll=0;
    snprintf(s_kind,sizeof s_kind,"review");
    signal_utf8_copy(s_prompt,sizeof s_prompt,prompt->value->cstring);
    signal_utf8_copy(s_review_context,sizeof s_review_context,review_context->value->cstring);
    start_timeout(); redraw(); return;
  }
  if (id && command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"ask")) {
    if (id->value->uint32==s_request_id && (busy() || s_answer_id==s_request_id)) return;
    if (busy()) cancel_turn("");
    snprintf(s_kind,sizeof s_kind,"ask");
    s_request_id=id->value->uint32; s_view=VIEW_WAIT; s_prompt[0]='\0';
    snprintf(s_status,sizeof s_status,"Asking through the phone..."); start_timeout(); redraw(); return;
  }
  if (id && command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"cancel")) {
    if (id->value->uint32==s_request_id) { s_view=VIEW_READER; s_collecting=false; cancel_turn("Stopped from the phone."); }
    return;
  }
  if (id && command && command->type==TUPLE_CSTRING && (!strcmp(command->value->cstring,"survey") || !strcmp(command->value->cstring,"capture") || !strcmp(command->value->cstring,"record"))) {
    if (busy() && id->value->uint32!=s_request_id) cancel_turn("");
    if (!strcmp(command->value->cstring,"record")) {
      if (busy() && id->value->uint32==s_request_id) return;
      s_request_id=id->value->uint32; s_phone_record=true; s_configured=true; ask(); return;
    }
    snprintf(s_kind,sizeof s_kind,"%s",command->value->cstring);
    collect(id->value->uint32); return;
  }
  if (!id || id->value->uint32!=s_request_id) { redraw(); return; }
  Tuple *text=dict_find(iter,MESSAGE_KEY_ResponseText),*status=dict_find(iter,MESSAGE_KEY_StatusText);
  if (text && text->type==TUPLE_CSTRING && text->length<=901 && text->length>1) {
    if (s_answer_id==s_request_id) { s_ack_id=s_request_id; flush(NULL); return; }
    if (!busy() || s_view==VIEW_REVIEW) return;
    bool history=!strcmp(s_kind,"history");
    signal_utf8_copy(history?s_history:s_answer,TEXT_CAP,text->value->cstring); s_answer_id=s_request_id;
    s_view=history?VIEW_HISTORY:VIEW_READER; s_status[0]='\0'; s_scroll=0; clear_timeout(); stop_sampling(); s_collecting=s_snapshot_pending=false;
    s_ack_id=s_request_id; flush(NULL); redraw(); return;
  }
  if (status && status->type==TUPLE_CSTRING && busy()) {
    signal_utf8_copy(s_status,sizeof s_status,status->value->cstring);
    t=dict_find(iter,MESSAGE_KEY_Complete);
    if (t && t->value->uint32) { s_view=VIEW_READER; clear_timeout(); stop_sampling(); s_collecting=s_snapshot_pending=false; }
  }
  redraw();
}
#ifdef PBL_MICROPHONE
static void dictated(DictationSession *session,DictationSessionStatus status,char *text,void *context) {
  if (s_view!=VIEW_DICTATION) return;
  if (status!=DictationSessionStatusSuccess || !text || !text[0]) { cancel_turn("Question cancelled or unavailable. Check speech settings."); return; }
  if (strlen(text)>400) { cancel_turn("Question is too long. Please use a shorter question."); return; }
  request("ask",text);
}
#endif
static void ask(void) {
  if (busy()) { cancel_turn("Stopped."); return; }
  if (!s_configured || !s_connected) { s_phone_record=false; s_view=VIEW_READER; snprintf(s_status,sizeof s_status,"Open Signal Station in the lab companion and configure a provider."); redraw(); return; }
#ifdef PBL_MICROPHONE
  if (!s_dictation) s_dictation=dictation_session_create(401,dictated,NULL);
  if (!s_dictation) { snprintf(s_status,sizeof s_status,"Dictation is unavailable."); s_view=VIEW_READER; redraw(); return; }
  dictation_session_enable_confirmation(s_dictation,s_confirm); dictation_session_enable_error_dialogs(s_dictation,false);
  s_view=VIEW_DICTATION; s_scroll=0; snprintf(s_status,sizeof s_status,"Speak near the watch. Back cancels.");
  if (dictation_session_start(s_dictation)!=DictationSessionStatusSuccess) cancel_turn("Dictation could not start. Check the phone.");
#else
  s_view=VIEW_READER; snprintf(s_status,sizeof s_status,"This watch has no microphone. Ask from the phone.");
#endif
  redraw();
}
static void local_action(const char *kind) {
  if (!s_connected || !s_bridge_ready) {
    s_view=VIEW_READER; s_scroll=0;
    snprintf(s_status,sizeof s_status,"Open Signal Station in the lab companion. No answer provider is needed for Capture or History.");
    s_ready_pending=true; flush(NULL); redraw(); return;
  }
  request(kind,NULL);
}
static void select_click(ClickRecognizerRef r,void *context) {
  if (s_view==VIEW_REVIEW) {
    if (!s_connected || !s_bridge_ready) { cancel_turn("Phone disconnected. Review the draft on your phone."); return; }
    // Retain the native ID and send no text; the phone owns the immutable draft.
    s_phone_record=true; request("confirm-wake",NULL);
  } else ask();
}
static void select_long(ClickRecognizerRef r,void *context) {
  if (s_view==VIEW_REVIEW) return;
  if (s_view==VIEW_MENU) { s_view=VIEW_HELP; s_scroll=0; redraw(); }
  else ask();
}
static void up_click(ClickRecognizerRef r,void *context) {
  if (s_view==VIEW_MENU) local_action("capture");
  else { s_scroll-=36; if (s_scroll<0) s_scroll=0; } redraw();
}
static void down_click(ClickRecognizerRef r,void *context) {
  if (s_view==VIEW_MENU) local_action("history");
  else { s_scroll+=36; if (s_scroll>s_scroll_max) s_scroll=s_scroll_max; } redraw();
}
static void back_click(ClickRecognizerRef r,void *context) {
  if (busy()) { cancel_turn(""); s_view=VIEW_MENU; s_scroll=0; redraw(); }
  else if (s_view!=VIEW_MENU) { s_view=VIEW_MENU; s_scroll=0; redraw(); }
  else window_stack_pop(true);
}
static void clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT,select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT,650,select_long,NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_UP,150,up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN,150,down_click);
  window_single_click_subscribe(BUTTON_ID_BACK,back_click);
}
static GRect body_bounds(GRect b) { int inset=PBL_IF_ROUND_ELSE(b.size.w/7,7); return GRect(inset,38,b.size.w-2*inset,b.size.h-76); }
static GFont font(void) { return fonts_get_system_font(layer_get_bounds(s_canvas).size.h>=200 ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD); }
static const char *body_text(void) {
  if (s_view==VIEW_HELP) return "Home shortcuts\nUp: Capture\nSelect: Ask\nDown: History\n\nCapture saves selected readings on your phone without a language model request.\nHistory reads recent saved records without a provider.\nAsk speaks a question and uses your phone's answer provider.\n\nUp/Down scroll reports and history. Back cancels or returns home. Hold Select here to ask.\n\nChoose sources, manage saved history, and configure providers on the phone.";
  if (s_view==VIEW_REVIEW) { snprintf(s_display,sizeof s_display,"%s\n\n%s\n\nSelect: Send\nBack: keep on phone",s_prompt,s_review_context); return s_display; }
  if (s_view==VIEW_HISTORY) return s_history;
  if (s_view==VIEW_WAIT) { snprintf(s_display,sizeof s_display,"%s%s%s",s_prompt[0]?s_prompt:"",s_prompt[0]?"\n\n":"",s_status); return s_display; }
  if (s_view==VIEW_DICTATION) return s_status;
  if (s_status[0]) { snprintf(s_display,sizeof s_display,"%s%s%s",s_status,s_answer[0]?"\n\n":"",s_answer); return s_display; }
  return s_answer[0]?s_answer:"No report yet. Back returns home.";
}
static void draw_body(Layer *layer,GContext *ctx) {
  GRect b=layer_get_bounds(layer); graphics_context_set_text_color(ctx,GColorWhite);
  if (s_view==VIEW_MENU) {
    int h=b.size.h/3;
    for (int i=0;i<3;i++) {
      int cy=i*h+h/2;
      graphics_context_set_stroke_color(ctx,PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite));
      graphics_context_set_fill_color(ctx,PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite));
      if (i==0) { // Capture: a receiving ring and central sample.
        graphics_draw_circle(ctx,GPoint(10,cy),8);
        graphics_fill_circle(ctx,GPoint(10,cy),3);
      } else if (i==1) { // Ask: speech bubble.
        graphics_draw_round_rect(ctx,GRect(2,cy-7,17,12),3);
        graphics_draw_line(ctx,GPoint(5,cy+5),GPoint(5,cy+9));
        graphics_draw_line(ctx,GPoint(5,cy+9),GPoint(10,cy+5));
      } else { // History: three saved lines.
        for (int j=-1;j<=1;j++) graphics_draw_line(ctx,GPoint(3,cy+j*5),GPoint(18,cy+j*5));
      }
      graphics_context_set_text_color(ctx,GColorWhite);
      graphics_draw_text(ctx,s_items[i],font(),GRect(27,i*h-2,b.size.w-27,h),GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
    }
    return;
  }
  GSize size=graphics_text_layout_get_content_size(body_text(),font(),GRect(0,0,b.size.w,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft);
  s_scroll_max=size.h>b.size.h?size.h-b.size.h:0; if (s_scroll>s_scroll_max) s_scroll=s_scroll_max;
  graphics_draw_text(ctx,body_text(),font(),GRect(0,-s_scroll,b.size.w,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft,NULL);
}
static void draw(Layer *layer,GContext *ctx) {
  GRect b=layer_get_bounds(layer); int inset=PBL_IF_ROUND_ELSE(b.size.w/7,7);
  graphics_context_set_fill_color(ctx,GColorBlack); graphics_fill_rect(ctx,b,0,GCornerNone);
  graphics_context_set_text_color(ctx,PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite));
  graphics_draw_text(ctx,s_view==VIEW_MENU?"SIGNAL STATION":s_view==VIEW_HELP?"FIELD MANUAL":s_view==VIEW_REVIEW?"REVIEW DRAFT":s_view==VIEW_DICTATION?"LISTENING":s_view==VIEW_HISTORY?"RECENT HISTORY":busy()?"CONTACTING":"FIELD REPORT",fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),GRect(inset,9,b.size.w-inset*2,24),GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
  graphics_context_set_stroke_color(ctx,PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite)); graphics_draw_line(ctx,GPoint(inset,34),GPoint(b.size.w-inset,34));
  graphics_context_set_text_color(ctx,GColorWhite);
  const char *footer=s_view==VIEW_REVIEW?"Select: Send | Back: keep":busy()?"Back: stop":s_view==VIEW_MENU?(s_connected?(s_bridge_ready?"Hold Select: help":"Open lab companion"):"Phone disconnected"):"Up/Down: read";
  graphics_draw_text(ctx,footer,fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(inset,b.size.h-32,b.size.w-2*inset,20),GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
}
static void redraw(void) { if (s_canvas) layer_mark_dirty(s_canvas); if (s_body) layer_mark_dirty(s_body); }
static void connection_changed(bool connected) {
  s_connected=connected; if (!connected) s_bridge_ready=false;
  if (!connected && busy()) cancel_turn("Phone connection lost. Reconnect to ask again.");
  if (connected) { s_ready_pending=true; flush(NULL); } redraw();
}
static void window_load(Window *window) {
  Layer *root=window_get_root_layer(window); GRect b=layer_get_bounds(root);
  s_canvas=layer_create(b); layer_set_update_proc(s_canvas,draw); layer_add_child(root,s_canvas);
  s_body=layer_create(body_bounds(b)); layer_set_update_proc(s_body,draw_body); layer_add_child(root,s_body);
}
static void window_unload(Window *window) { layer_destroy(s_body); s_body=NULL; layer_destroy(s_canvas); s_canvas=NULL; }
static void init(void) {
  s_request_id=persist_exists(PERSIST_REQUEST_ID)?(uint32_t)persist_read_int(PERSIST_REQUEST_ID):(uint32_t)time(NULL);
  s_window=window_create(); window_set_background_color(s_window,GColorBlack);
  window_set_window_handlers(s_window,(WindowHandlers){.load=window_load,.unload=window_unload}); window_set_click_config_provider(s_window,clicks);
  app_message_register_inbox_received(inbox); app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent); app_message_register_outbox_failed(outbox_failed); app_message_open(2048,2048);
  s_connected=connection_service_peek_pebble_app_connection(); connection_service_subscribe((ConnectionHandlers){.pebble_app_connection_handler=connection_changed});
  window_stack_push(s_window,true); s_ready_pending=true; flush(NULL);
}
static void deinit(void) {
  stop_sampling(); clear_timeout(); if (s_outbox_timer) app_timer_cancel(s_outbox_timer);
#ifdef PBL_MICROPHONE
  if (s_dictation) dictation_session_destroy(s_dictation);
#endif
  connection_service_unsubscribe(); app_message_deregister_callbacks(); window_destroy(s_window);
}
int main(void) { init(); app_event_loop(); deinit(); }
