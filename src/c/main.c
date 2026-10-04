// Signal Station — Luke Steuber. Speak, survey, and read.
#include <pebble.h>
#include "signal_math.h"
#include "signal_history.h"
#include "signal_markdown.h"
#include "signal_home.h"
#include "signal_transport.h"
#define TEXT_CAP 1024
#define SNAPSHOT_CAP 1900
#define PERSIST_REQUEST_ID 1
typedef enum { VIEW_MENU, VIEW_READER, VIEW_HELP, VIEW_WAIT, VIEW_DICTATION, VIEW_HISTORY, VIEW_REVIEW, VIEW_HOME_LIST, VIEW_HOME_DETAIL, VIEW_HOME_REVIEW, VIEW_HOME_RESULT, VIEW_HOME_HANDOFF, VIEW_QUESTION_DRAFT, VIEW_QUESTION_SYSTEMS, VIEW_QUESTION_REVIEW, VIEW_QUESTION_PHONE } View;
static Window *s_window;
static Layer *s_canvas, *s_body;
static View s_view;
static char s_answer[TEXT_CAP], s_history[TEXT_CAP], s_status[160], s_display[TEXT_CAP+200], s_prompt[401], s_review_context[351];
static char s_enabled[900], s_snapshot[SNAPSHOT_CAP], s_kind[24];
static bool s_configured, s_bridge_ready, s_confirm, s_connected, s_collecting, s_sampling, s_phone_record;
static int s_scroll, s_scroll_max, s_stage;
static uint32_t s_request_id, s_answer_id, s_cancel_id, s_ack_id;
static uint32_t s_collection_id, s_handoff_id;
static char s_phone_hint[100];
static bool s_home_available;
static SignalHomePage s_home_page;
static SignalHomeIntent s_home_intent, s_home_wire, s_home_cancel;
static uint32_t s_home_reply_id, s_home_cancel_id;
static uint16_t s_home_requested_page;
static int s_home_selected;
static bool s_home_review_read;
static char s_home_text[SIGNAL_HOME_TEXT_CAP];
static bool s_question_available,s_question_read,s_question_has_prompt;
static char s_bridge_session[65],s_question_draft[65],s_question_review[65],s_question_text[901];
static char s_context_id[65],s_context_kind[8]="none",s_record_id[65],s_record_kind[8];
static char s_question_mode[8]="none",s_question_system[65];
static bool s_question_selected;
static uint32_t s_question_revision,s_question_expires,s_question_reply_id;
static SignalHomePage s_question_systems;
static int s_question_row;
static uint16_t s_question_page;
static bool s_request_pending, s_ready_pending, s_clear_pending, s_settings_pending, s_snapshot_pending;
static bool s_outbox_busy, s_snapshot_complete;
static int s_outbox_kind;
static SignalTransport s_transport;
static uint32_t s_outbox_id;
static AppTimer *s_outbox_timer, *s_timeout_timer, *s_sample_timer;
static SignalMotion s_motion;
static CompassHeadingData s_compass;
static uint64_t s_compass_received_ms;
static time_t s_collected_at;
#ifdef PBL_MICROPHONE
static DictationSession *s_dictation;
#endif
static const char *s_items[] = {"Capture", "Ask", "History"};
static void redraw(void);
static void clicks(void *context);
static bool s_menu_clicks;
static void flush(void *unused);
static void next_snapshot(void);
static void ask(void);
static void measure_body(void);
static bool busy(void) { return s_view == VIEW_WAIT || s_view == VIEW_DICTATION || s_view == VIEW_REVIEW || s_view == VIEW_HOME_REVIEW || s_view==VIEW_QUESTION_REVIEW; }
static bool home_view(void) { return (s_view>=VIEW_HOME_LIST && s_view<=VIEW_HOME_HANDOFF) || (s_view==VIEW_WAIT && !strncmp(s_kind,"home-",5)); }
static void home_cancel(void);
static void home_request(const char *kind);
static void question_request(const char *kind);
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
  if (home_view()) home_cancel();
  else if (s_view == VIEW_WAIT || s_view == VIEW_REVIEW || s_collecting || (s_view == VIEW_DICTATION && s_phone_record)) s_cancel_id = s_request_id;
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
  signal_transport_resume(&s_transport);
  if (!s_phone_record) next_request();
  s_phone_record=false; s_view = VIEW_WAIT; s_scroll = 0;
  snprintf(s_kind, sizeof s_kind, "%s", kind);
  signal_utf8_copy(s_prompt, sizeof s_prompt, prompt ? prompt : "");
  snprintf(s_status, sizeof s_status, "%s", !strcmp(kind,"history") ? "Loading saved history..." : !strcmp(kind,"capture") ? "Saving selected readings..." : !strcmp(kind,"survey") ? "Surveying selected sources..." : "Asking through the phone...");
  s_request_pending = true; start_timeout(); flush(NULL); redraw();
}
static void retry_flush(void) { if (!s_outbox_timer) s_outbox_timer = app_timer_register(100, flush, NULL); }
static void home_cancel(void) {
  s_home_cancel_id=s_request_id; s_home_cancel=s_home_wire;
  if (s_home_intent.intent[0]) s_home_cancel=s_home_intent;
  s_home_intent.consumed=true;
}
static void home_request(const char *kind) {
  if (!s_connected || !s_bridge_ready || !s_home_available) {
    clear_timeout(); s_view=VIEW_HOME_RESULT; strcpy(s_home_text,"Open or update Signal Station on your phone to use Home favorites."); redraw(); return;
  }
  if ((!strcmp(kind,"home-review") || !strcmp(kind,"home-confirm")) && !s_question_available) {
    s_home_intent.consumed=true; s_view=VIEW_HOME_HANDOFF;
    strcpy(s_home_text,"Update Signal Station on your phone before reviewing an action on the watch. Continue on the phone."); redraw(); return;
  }
  s_home_wire=s_home_intent;
  signal_transport_resume(&s_transport);
  next_request(); s_phone_record=false; s_view=VIEW_WAIT; s_scroll=0; s_prompt[0]=0;
  snprintf(s_kind,sizeof s_kind,"%s",kind);
  strcpy(s_status,"Contacting Home on the phone..."); s_request_pending=true;
  start_timeout(); flush(NULL); redraw();
}
static void home_list(uint16_t page) {
  memset(&s_home_intent,0,sizeof s_home_intent); s_home_requested_page=page; home_request("home-list");
}
static void home_expired(void *unused) {
  s_timeout_timer=NULL; home_cancel(); s_view=VIEW_HOME_RESULT;
  strcpy(s_home_text,"Review expired. Open the favorite again for a new review."); s_scroll=0; flush(NULL); redraw();
}
static void home_binding(DictionaryIterator *iter,const SignalHomeIntent *binding) {
  dict_write_uint8(iter,MESSAGE_KEY_HomeVersion,SIGNAL_HOME_VERSION);
  if (binding->favorite[0]) dict_write_cstring(iter,MESSAGE_KEY_HomeFavorite,binding->favorite);
  if (binding->action[0]) dict_write_cstring(iter,MESSAGE_KEY_HomeAction,binding->action);
  if (binding->intent[0]) dict_write_cstring(iter,MESSAGE_KEY_HomeIntent,binding->intent);
}
static bool question_view(void) { return s_view>=VIEW_QUESTION_DRAFT || (s_view==VIEW_WAIT && !strncmp(s_kind,"question-",9)); }
static void fresh_question(bool contextual) {
  strcpy(s_context_kind,contextual?s_record_kind:"none"); strcpy(s_context_id,contextual?s_record_id:"");
  s_question_draft[0]=s_question_review[0]=0; strcpy(s_question_mode,"none"); s_question_has_prompt=false;
}
static void question_request(const char *kind) {
  if (!s_connected || !s_bridge_ready || !s_question_available) {
    s_view=VIEW_QUESTION_PHONE; strcpy(s_question_text,"Open or update Signal Station on the phone to review this question."); redraw(); return;
  }
  signal_transport_resume(&s_transport); next_request(); s_phone_record=false; s_scroll=0;
  s_view=VIEW_WAIT; snprintf(s_kind,sizeof s_kind,"%s",kind);
  strcpy(s_status,"Preparing the exact question on your phone...");
  s_request_pending=true; start_timeout(); flush(NULL); redraw();
}
static void question_binding(DictionaryIterator *iter) {
  dict_write_uint8(iter,MESSAGE_KEY_QuestionReviewVersion,1);
  if (!strcmp(s_kind,"question-open")) {
    dict_write_cstring(iter,MESSAGE_KEY_Prompt,s_prompt);
    dict_write_cstring(iter,MESSAGE_KEY_QuestionContextKind,s_context_kind);
    if (s_context_id[0]) dict_write_cstring(iter,MESSAGE_KEY_QuestionContextId,s_context_id);
    if(s_question_draft[0]) { dict_write_cstring(iter,MESSAGE_KEY_QuestionDraft,s_question_draft); dict_write_uint32(iter,MESSAGE_KEY_QuestionRevision,s_question_revision); }
  } else {
    dict_write_cstring(iter,MESSAGE_KEY_QuestionDraft,s_question_draft);
    dict_write_uint32(iter,MESSAGE_KEY_QuestionRevision,s_question_revision);
  }
  if (!strcmp(s_kind,"question-send")) dict_write_cstring(iter,MESSAGE_KEY_QuestionReview,s_question_review);
  if (!strcmp(s_kind,"question-home")) {
    dict_write_cstring(iter,MESSAGE_KEY_QuestionHomeMode,s_question_mode);
    if (s_question_system[0]) { dict_write_cstring(iter,MESSAGE_KEY_QuestionSystem,s_question_system); dict_write_uint8(iter,MESSAGE_KEY_QuestionSelected,s_question_selected); }
  }
  if (!strcmp(s_kind,"question-systems")) dict_write_uint16(iter,MESSAGE_KEY_QuestionPage,s_question_page);
}
static void transport_failed(void) {
  if (signal_transport_failed(&s_transport,s_outbox_kind,s_outbox_id)) { retry_flush(); return; }
  s_request_pending=s_snapshot_pending=s_collecting=false;
  s_ready_pending=s_clear_pending=s_settings_pending=false;
  // Keep only recovery traffic pending, paused until explicit retry/reconnection.
  if (s_outbox_kind==8) { s_handoff_id=0; strcpy(s_phone_hint,"Phone unavailable. Hold Select to retry."); }
  stop_sampling(); clear_timeout(); s_view=VIEW_READER;
  strcpy(s_status,"Phone unavailable. Reconnect or try again."); redraw();
}
// One in-flight message. Cancel and text acknowledgement always take priority.
static void flush(void *unused) {
  s_outbox_timer = NULL;
  if (s_outbox_busy || s_transport.paused) return;
  int kind = s_cancel_id ? 1 : s_home_cancel_id ? 9 : s_ack_id ? 2 : s_clear_pending ? 3 : s_settings_pending ? 4 :
    s_ready_pending ? 5 : s_request_pending ? 6 : s_snapshot_pending ? 7 : s_handoff_id ? 8 : 0;
  if (!kind) return;
  s_outbox_kind = kind;
  s_outbox_id=kind==1?s_cancel_id:kind==2?s_ack_id:kind==8?s_handoff_id:kind==9?s_home_cancel_id:s_request_id;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) { transport_failed(); return; }
  if (kind == 1) { dict_write_cstring(iter,MESSAGE_KEY_RequestType,"cancel"); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_cancel_id); }
  if (kind == 2) { dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_ack_id); dict_write_uint8(iter,MESSAGE_KEY_TextAck,1); }
  if (kind >= 3 && kind <= 5) dict_write_cstring(iter,MESSAGE_KEY_RequestType,kind==3 ? "clear" : kind==4 ? "settings" : "ready");
  if (kind == 5) { dict_write_uint8(iter,MESSAGE_KEY_HomeVersion,SIGNAL_HOME_VERSION); dict_write_uint8(iter,MESSAGE_KEY_QuestionReviewVersion,1); }
  if (kind == 9) { dict_write_cstring(iter,MESSAGE_KEY_RequestType,"home-cancel"); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_home_cancel_id); home_binding(iter,&s_home_cancel); }
  if (kind == 6) {
    dict_write_cstring(iter,MESSAGE_KEY_RequestType,s_kind); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_request_id);
    if (s_prompt[0] && strncmp(s_kind,"question-",9)) dict_write_cstring(iter,MESSAGE_KEY_Prompt,s_prompt);
    if (!strncmp(s_kind,"question-",9)) question_binding(iter);
    if (!strncmp(s_kind,"home-",5)) { home_binding(iter,&s_home_wire); if (!strcmp(s_kind,"home-list")) dict_write_uint16(iter,MESSAGE_KEY_HomePage,s_home_requested_page); }
  }
  if (kind == 7) {
    dict_write_cstring(iter,MESSAGE_KEY_RequestType,"watch-data"); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_request_id);
    dict_write_cstring(iter,MESSAGE_KEY_Snapshot,s_snapshot); dict_write_uint8(iter,MESSAGE_KEY_Complete,s_snapshot_complete);
  }
  if (kind == 8) { dict_write_cstring(iter,MESSAGE_KEY_RequestType,"continue-phone"); dict_write_uint32(iter,MESSAGE_KEY_RequestId,s_handoff_id); }
  if (app_message_outbox_send() == APP_MSG_OK) s_outbox_busy = true;
  else transport_failed();
}
static void outbox_sent(DictionaryIterator *iter, void *context) {
  s_outbox_busy = false; signal_transport_sent(&s_transport);
  if (s_outbox_kind == 1) { Tuple *t = dict_find(iter,MESSAGE_KEY_RequestId); if (t && t->value->uint32==s_cancel_id) s_cancel_id=0; }
  if (s_outbox_kind == 2) { Tuple *t = dict_find(iter,MESSAGE_KEY_RequestId); if (t && t->value->uint32==s_ack_id) s_ack_id=0; }
  if (s_outbox_kind == 3) s_clear_pending=false;
  if (s_outbox_kind == 4) s_settings_pending=false;
  if (s_outbox_kind == 5) s_ready_pending=false;
  if (s_outbox_kind == 6 && s_outbox_id==s_request_id) s_request_pending=false;
  if (s_outbox_kind == 7 && s_outbox_id==s_request_id) { s_snapshot_pending=false; if (s_collecting) next_snapshot(); }
  if (s_outbox_kind == 8) s_handoff_id=0;
  if (s_outbox_kind == 9) { Tuple *t=dict_find(iter,MESSAGE_KEY_RequestId); if (t && t->value->uint32==s_home_cancel_id) s_home_cancel_id=0; }
  flush(NULL);
}
static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  s_outbox_busy=false;
  if ((s_outbox_kind==6 || s_outbox_kind==7) && s_outbox_id!=s_request_id) { signal_transport_resume(&s_transport); flush(NULL); return; }
  transport_failed();
}
static void inbox_dropped(AppMessageResult reason, void *context) { cancel_turn("Incomplete phone message. Try again."); }
static void accel(AccelData *samples, uint32_t count) {
  if (!s_sampling) return;
  for (uint32_t i=0; i<count; i++) signal_motion_add(&s_motion,samples[i].x,samples[i].y,samples[i].z,samples[i].did_vibrate,samples[i].timestamp);
}
static void compass(CompassHeadingData data) {
  if (!s_sampling) return;
  time_t seconds; uint16_t milliseconds;
  time_ms(&seconds,&milliseconds);
  s_compass=data; s_compass_received_ms=(uint64_t)seconds*1000+milliseconds;
}
static void append_observation_ms(const char *key, const char *value, const char *unit, const char *status,
                                  const char *period, int64_t from, int64_t end, bool measured,
                                  const char *fields, bool date) {
  if (!enabled(key)) return;
  size_t n=strlen(s_snapshot); char date_text[40]="", time_fields[180]="";
  if (date) { time_t seconds=from/1000; char day[16]; strftime(day,sizeof day,"%Y-%m-%d",localtime(&seconds)); snprintf(date_text,sizeof date_text,",\"date\":\"%s\"",day); }
  if (from>=0 && end>=0) {
    snprintf(time_fields,sizeof time_fields,",\"windowStart\":%llu,\"windowEnd\":%llu",(unsigned long long)from,(unsigned long long)end);
    if (measured) { size_t size=strlen(time_fields); snprintf(time_fields+size,sizeof time_fields-size,",\"measuredAt\":%llu",(unsigned long long)end); }
  }
  snprintf(s_snapshot+n,sizeof s_snapshot-n,
    "%s{\"key\":\"%s\",\"source\":\"watch\",\"value\":%s,\"unit\":\"%s\",\"collectedAt\":%ld000,\"status\":\"%s\",\"period\":\"%s\"%s%s%s%s}",
    n>1 ? "," : "",key,value,unit,(long)s_collected_at,status,period,time_fields,date_text,fields?",\"fields\":" : "",fields?fields:"");
}
static void append_observation(const char *key, const char *value, const char *unit, const char *status,
                               const char *period, time_t from, time_t end, bool date) {
  bool known=strcmp(status,"timestamp_unknown")!=0;
  // An attempted collection window is not proof that a measurement exists.
  bool measured=known && strcmp(value,"null")!=0 && (!strcmp(status,"fresh") || !strcmp(status,"available"));
  append_observation_ms(key,value,unit,status,period,known?(int64_t)from*1000:-1,known?(int64_t)end*1000:-1,measured,NULL,date);
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
static void minute_history(void) {
  if (!enabled("watch.minute_history")) return;
  time_t requested_end=s_collected_at-s_collected_at%60,requested_start=requested_end-SIGNAL_HISTORY_MINUTES*60;
  time_t start=requested_start,end=requested_end;
  SignalMinute minutes[SIGNAL_HISTORY_MINUTES]={0};
  uint32_t count=0;
  const char *status="not_supported", *fields=NULL;
#ifdef PBL_HEALTH
  HealthServiceAccessibilityMask mask=health_service_metric_accessible(HealthMetricStepCount,requested_start,requested_end);
  status=mask & HealthServiceAccessibilityMaskNoPermission ? "permission_denied" : "unavailable";
  if (!(mask & HealthServiceAccessibilityMaskNoPermission)) {
    HealthMinuteData records[SIGNAL_HISTORY_MINUTES]={0};
    count=health_service_get_minute_history(records,SIGNAL_HISTORY_MINUTES,&start,&end);
    // Some firmware shifts start to its first available record without reducing
    // the requested record count. Preserve only completed minutes inside the
    // requested interval; validate the SDK's count and span before indexing it.
    if (count && count<=SIGNAL_HISTORY_MINUTES && start>=requested_start &&
        start%60==0 && (int64_t)end-start==(int64_t)count*60 && end>requested_end) {
      uint32_t retained=start<requested_end ? (uint32_t)(requested_end-start)/60 : 0;
      static char clipping[240];
      snprintf(clipping,sizeof clipping,
        "{\"reason\":\"history_window_clipped\",\"sdk_returned_minutes\":\"%lu\","
        "\"sdk_window_start_s\":\"%ld\",\"sdk_window_end_s\":\"%ld\","
        "\"excluded_after_requested_minutes\":\"%lu\"}",
        (unsigned long)count,(long)start,(long)end,(unsigned long)(count-retained));
      fields=clipping; count=retained; end=requested_end;
    }
    if (!signal_history_bounds(count,(int64_t)start*1000,(int64_t)end*1000,(int64_t)requested_start*1000,(int64_t)requested_end*1000)) {
      count=0; fields="{\"reason\":\"invalid_history_window\"}";
    }
    for (uint32_t i=0;i<count;i++) minutes[i]=(SignalMinute){.steps=records[i].steps,.vmc=records[i].vmc,
      .orientation=records[i].orientation,.light=records[i].light,.heart_rate_bpm=records[i].heart_rate_bpm,.invalid=records[i].is_invalid};
    if (signal_history_valid(minutes,count)) status="available";
  }
#endif
  // Collection runs on the app thread. Keep its bounded formatter off the small
  // callback stack, including when the compiler inlines it into next_snapshot.
  static char value[SIGNAL_HISTORY_VALUE_CAP];
  if (!signal_history_format(value,sizeof value,minutes,count,(int64_t)start*1000,(int64_t)end*1000,(int64_t)requested_start*1000,(int64_t)requested_end*1000)) {
    cancel_turn("Watch history exceeded its limit."); return;
  }
  append_observation_ms("watch.minute_history",value,"minute_records",status,"recent_15_minutes",
    count?(int64_t)start*1000:-1,count?(int64_t)end*1000:-1,signal_history_valid(minutes,count)>0,fields,false);
}
static void motion_observation(void) {
  if (!enabled("watch.motion")) return;
  static char value[620];
  snprintf(value,sizeof value,
    "{\"samples\":%lu,\"mean_x\":%ld,\"mean_y\":%ld,\"mean_z\":%ld,\"peak_abs_axis\":%d,\"variance_mg2\":%lu,"
    "\"received_samples\":%lu,\"vibration_excluded\":%lu,\"timestamp_rejected\":%lu,\"capacity_excluded\":%lu,"
    "\"first_sample_ms\":%llu,\"last_sample_ms\":%llu,\"requested_hz\":10,\"requested_duration_ms\":5000,\"timing\":\"sdk_epoch_ms\"}",
    (unsigned long)s_motion.count,(long)(s_motion.count?s_motion.x/(int)s_motion.count:0),
    (long)(s_motion.count?s_motion.y/(int)s_motion.count:0),(long)(s_motion.count?s_motion.z/(int)s_motion.count:0),s_motion.peak,
    (unsigned long)signal_motion_variance(&s_motion),(unsigned long)s_motion.received,(unsigned long)s_motion.vibration_excluded,
    (unsigned long)s_motion.timestamp_rejected,(unsigned long)s_motion.capacity_excluded,
    (unsigned long long)s_motion.first_ms,(unsigned long long)s_motion.last_ms);
  append_observation_ms("watch.motion",value,"mg",s_motion.count?"fresh":"unavailable","5_second_sample",
    s_motion.count?(int64_t)s_motion.first_ms:-1,s_motion.count?(int64_t)s_motion.last_ms:-1,s_motion.count>0,NULL,false);
}
static time_t day_start(int ago) {
  struct tm day=*localtime(&s_collected_at); day.tm_hour=day.tm_min=day.tm_sec=0; day.tm_mday-=ago; day.tm_isdst=-1;
  return mktime(&day); // Local calendar days preserve daylight-saving boundaries.
}
static void next_snapshot(void) {
  if (!s_collecting || s_snapshot_pending) return;
  // Disabled sources must not consume another stack frame per empty batch.
  while (s_collecting && !s_snapshot_pending) {
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
      minute_history();
      if (!s_collecting) return;
    } else if (stage==20) {
      if (s_sampling) { s_stage--; return; }
      motion_observation();
      snprintf(value,sizeof value,"%ld",(long)(((TRIG_MAX_ANGLE-s_compass.magnetic_heading)*360LL/TRIG_MAX_ANGLE)%360));
      bool calibrated=s_compass.compass_status==CompassStatusCalibrated;
      char fields[100]; snprintf(fields,sizeof fields,"{\"timing\":\"callback_received\",\"received_at_ms\":\"%llu\"}",(unsigned long long)s_compass_received_ms);
      append_observation_ms("watch.compass",calibrated?value:"null","degrees_magnetic_clockwise",calibrated?"timestamp_unknown":s_compass.compass_status==CompassStatusCalibrating?"calibrating":"unavailable","current",-1,-1,false,s_compass_received_ms?fields:NULL,false);
    }
    if (strlen(s_snapshot)+2>=sizeof s_snapshot) { cancel_turn("Watch readings exceeded their limit."); return; }
    strcat(s_snapshot,"]"); s_snapshot_complete=stage>=20;
    if (strlen(s_snapshot)==2 && !s_snapshot_complete) continue;
    s_snapshot_pending=true;
    if (s_snapshot_complete) s_collecting=false;
    flush(NULL);
    return;
  }
}
static void sample_done(void *unused) { s_sample_timer=NULL; stop_sampling(); if (s_collecting && s_stage==20) next_snapshot(); }
static void collect(uint32_t id) {
  if (id==s_collection_id) return;
  s_collection_id=id;
  stop_sampling(); s_request_id=id; s_view=VIEW_WAIT; s_collecting=true; s_stage=0;
  s_collected_at=time(NULL); memset(&s_motion,0,sizeof s_motion); s_compass.compass_status=CompassStatusUnavailable; s_compass_received_ms=0;
  if (enabled("watch.motion") || enabled("watch.compass")) {
    s_sampling=true;
    if (enabled("watch.motion")) { accel_data_service_subscribe(10,accel); accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ); }
    if (enabled("watch.compass")) compass_service_subscribe(compass);
    s_sample_timer=app_timer_register(5000,sample_done,NULL);
  }
  snprintf(s_status,sizeof s_status,"Collecting selected watch readings..."); start_timeout(); next_snapshot(); redraw();
}
static const char *home_string(DictionaryIterator *iter,uint32_t key,size_t cap) {
  Tuple *t=dict_find(iter,key);
  return t && t->type==TUPLE_CSTRING && signal_home_string(t->value->cstring,t->length,cap)?t->value->cstring:NULL;
}
static bool home_uint(DictionaryIterator *iter,uint32_t key,uint32_t *value) {
  Tuple *t=dict_find(iter,key);
  if (!t || (t->type!=TUPLE_UINT && t->type!=TUPLE_INT) || (t->length!=1 && t->length!=2 && t->length!=4)) return false;
  if (t->type==TUPLE_INT && (t->length==1?t->value->int8:t->length==2?t->value->int16:t->value->int32)<0) return false;
  *value=t->length==1?t->value->uint8:t->length==2?t->value->uint16:t->value->uint32; return true;
}
static void question_expired(void *unused) {
  s_timeout_timer=NULL; s_question_review[0]=s_question_draft[0]=0; s_question_read=false; strcpy(s_question_mode,"none");
  s_view=s_question_has_prompt?VIEW_QUESTION_DRAFT:VIEW_READER; s_question_row=0; s_scroll=0;
  strcpy(s_status,"Review expired. Open the voice draft on your phone or start a new question."); redraw();
}
static void question_receive(DictionaryIterator *iter,uint32_t id) {
  if (id==s_question_reply_id) { s_ack_id=id; flush(NULL); return; }
  if (id!=s_request_id || s_view!=VIEW_WAIT || strncmp(s_kind,"question-",9)) return;
  uint32_t version,revision;
  const char *mode=home_string(iter,MESSAGE_KEY_QuestionMode,16);
  const char *draft=home_string(iter,MESSAGE_KEY_QuestionDraft,65);
  const char *access=home_string(iter,MESSAGE_KEY_QuestionHomeMode,8);
  bool valid=home_uint(iter,MESSAGE_KEY_QuestionReviewVersion,&version) && version==1 && mode && draft && signal_home_id(draft) &&
    home_uint(iter,MESSAGE_KEY_QuestionRevision,&revision) && revision && access && (!strcmp(access,"none")||!strcmp(access,"read")||!strcmp(access,"actions")) &&
    (!strcmp(s_kind,"question-open") || (!strcmp(draft,s_question_draft) && revision>=s_question_revision));
  if (valid && !strcmp(mode,"systems")) {
    uint32_t page,pages; Tuple *items=dict_find(iter,MESSAGE_KEY_QuestionItems);
    valid=home_uint(iter,MESSAGE_KEY_QuestionPage,&page) && home_uint(iter,MESSAGE_KEY_QuestionPages,&pages) && page<1000 && pages<=1000 && items && items->type==TUPLE_CSTRING &&
      signal_home_page(&s_question_systems,items->value->cstring,items->length,(uint16_t)page,(uint16_t)pages);
    if(valid) { s_view=VIEW_QUESTION_SYSTEMS; s_question_row=0; }
  } else if (valid) {
    const char *text=home_string(iter,MESSAGE_KEY_ResponseText,901);
    valid=text && text[0];
    if(valid && !strcmp(mode,"review")) {
      const char *review=home_string(iter,MESSAGE_KEY_QuestionReview,65); uint32_t expires;
      valid=!strcmp(s_kind,"question-review") && review && signal_home_id(review) && home_uint(iter,MESSAGE_KEY_QuestionExpires,&expires) && expires>(uint32_t)time(NULL) && expires-(uint32_t)time(NULL)<=120;
      if(valid) { strcpy(s_question_review,review); s_question_expires=expires; s_question_read=false; s_view=VIEW_QUESTION_REVIEW; }
    } else if(valid && !strcmp(mode,"draft")) { s_view=VIEW_QUESTION_DRAFT; s_question_row=0; s_question_review[0]=0; }
    else if(valid && !strcmp(mode,"phone")) { s_view=VIEW_QUESTION_PHONE; s_question_review[0]=0; }
    else valid=false;
    if(valid) strcpy(s_question_text,text);
  }
  s_request_pending=false; clear_timeout(); s_scroll=0;
  if(valid) {
    strcpy(s_question_draft,draft); strcpy(s_question_mode,access); s_question_revision=revision; s_question_reply_id=id; s_ack_id=id;
    if(s_view==VIEW_QUESTION_REVIEW) s_timeout_timer=app_timer_register((s_question_expires-(uint32_t)time(NULL))*1000,question_expired,NULL);
  } else { s_question_review[0]=0; s_view=VIEW_QUESTION_PHONE; strcpy(s_question_text,"Question message could not be reviewed safely. Continue on the phone."); }
  bool refresh_systems=valid && !strcmp(s_kind,"question-home") && s_view==VIEW_QUESTION_DRAFT;
  flush(NULL); redraw();
  if(refresh_systems) question_request("question-systems");
}
static void home_receive(DictionaryIterator *iter,uint32_t id) {
  if (id==s_home_reply_id) { s_ack_id=id; flush(NULL); return; }
  if (id!=s_request_id || s_view!=VIEW_WAIT || strncmp(s_kind,"home-",5)) return;
  uint32_t version;
  const char *mode=home_string(iter,MESSAGE_KEY_HomeMode,16);
  bool valid=home_uint(iter,MESSAGE_KEY_HomeVersion,&version) && version==SIGNAL_HOME_VERSION && mode;
  if (valid && !strcmp(mode,"list")) {
    uint32_t page,pages; Tuple *items=dict_find(iter,MESSAGE_KEY_HomeItems);
    valid=!strcmp(s_kind,"home-list") && home_uint(iter,MESSAGE_KEY_HomePage,&page) && home_uint(iter,MESSAGE_KEY_HomePages,&pages) &&
      page==s_home_requested_page && page<1000 && pages<=1000 && items && items->type==TUPLE_CSTRING &&
      signal_home_page(&s_home_page,items->value->cstring,items->length,(uint16_t)page,(uint16_t)pages);
    if (valid) { s_home_selected=0; s_view=VIEW_HOME_LIST; }
  } else if (valid) {
    const char *text=home_string(iter,MESSAGE_KEY_ResponseText,SIGNAL_HOME_TEXT_CAP);
    const char *favorite=home_string(iter,MESSAGE_KEY_HomeFavorite,SIGNAL_HOME_ID_CAP);
    const char *action=home_string(iter,MESSAGE_KEY_HomeAction,SIGNAL_HOME_ID_CAP);
    valid=text && text[0] && favorite && !strcmp(favorite,s_home_wire.favorite);
    if (valid && !strcmp(mode,"review")) {
      const char *intent=home_string(iter,MESSAGE_KEY_HomeIntent,SIGNAL_HOME_ID_CAP); uint32_t expires;
      valid=!strcmp(s_kind,"home-review") && action && !strcmp(action,s_home_wire.action) && intent &&
        home_uint(iter,MESSAGE_KEY_HomeExpires,&expires) && signal_home_intent(&s_home_intent,favorite,action,intent,expires,(uint32_t)time(NULL));
      if (valid) { s_home_review_read=false; s_view=VIEW_HOME_REVIEW; }
    } else if (valid && !strcmp(mode,"detail")) {
      valid=!strcmp(s_kind,"home-open") && (!action || signal_home_id(action));
      if (valid) { memset(&s_home_intent,0,sizeof s_home_intent); strcpy(s_home_intent.favorite,favorite); if (action) strcpy(s_home_intent.action,action); s_view=VIEW_HOME_DETAIL; }
    } else if (valid && (!strcmp(mode,"handoff") || !strcmp(mode,"result"))) {
      s_home_intent.consumed=true; s_view=!strcmp(mode,"handoff")?VIEW_HOME_HANDOFF:VIEW_HOME_RESULT;
    } else valid=false;
    if (valid) strcpy(s_home_text,text);
  }
  if (!valid) {
    home_cancel(); s_view=VIEW_HOME_RESULT;
    strcpy(s_home_text,"Home message could not be reviewed safely. Continue on the phone.");
  } else { s_home_reply_id=id; s_ack_id=id; }
  s_request_pending=false; clear_timeout(); s_scroll=0;
  if (valid && s_view==VIEW_HOME_REVIEW) {
    uint32_t now=(uint32_t)time(NULL);
    if (!signal_home_can_confirm(&s_home_intent,now)) { home_expired(NULL); return; }
    s_timeout_timer=app_timer_register((s_home_intent.expires-now)*1000,home_expired,NULL);
  }
  flush(NULL); redraw();
}
static void inbox(DictionaryIterator *iter,void *context) {
  const char *session=home_string(iter,MESSAGE_KEY_BridgeSession,65);
  if (session && strcmp(session,s_bridge_session)) {
    bool replacement=s_bridge_session[0]!=0; strcpy(s_bridge_session,session);
    s_home_available=s_question_available=false; s_home_intent.consumed=true; s_question_review[0]=s_question_draft[0]=0;
    if(replacement && (busy() || home_view() || question_view())) {
      s_request_pending=s_snapshot_pending=s_collecting=false; s_cancel_id=s_home_cancel_id=s_ack_id=0;
      stop_sampling(); clear_timeout(); s_view=VIEW_READER; strcpy(s_status,"Phone restarted. Reopen the item for a fresh review.");
    }
  }
  uint32_t home_version;
  Tuple *home_cap=dict_find(iter,MESSAGE_KEY_HomeVersion);
  Tuple *t=dict_find(iter,MESSAGE_KEY_BridgeReady);
  bool home_supported=home_cap && home_uint(iter,MESSAGE_KEY_HomeVersion,&home_version) && home_version==SIGNAL_HOME_VERSION;
  s_home_available=signal_home_available_after_sync(s_home_available,t!=NULL,home_cap!=NULL,home_supported);
  if (t) s_bridge_ready=t->value->uint32!=0;
  uint32_t question_version;
  if (t || dict_find(iter,MESSAGE_KEY_QuestionReviewVersion)) s_question_available=home_uint(iter,MESSAGE_KEY_QuestionReviewVersion,&question_version) && question_version==1;
  t=dict_find(iter,MESSAGE_KEY_Configured); if (t) s_configured=t->value->uint32!=0;
  t=dict_find(iter,MESSAGE_KEY_Enabled); if (t && t->type==TUPLE_CSTRING) snprintf(s_enabled,sizeof s_enabled,"%s",t->value->cstring);
  t=dict_find(iter,MESSAGE_KEY_ConfirmTranscript); if (t) s_confirm=t->value->uint32!=0;
  Tuple *id=dict_find(iter,MESSAGE_KEY_RequestId),*command=dict_find(iter,MESSAGE_KEY_Command);
  if(command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"question-offer")) {
    uint32_t offer; if(!s_question_available || !home_uint(iter,MESSAGE_KEY_RequestId,&offer) || !offer || offer==s_question_reply_id || offer==s_request_id) return;
    if(busy()) cancel_turn("");
    s_request_id=offer; s_view=VIEW_WAIT; strcpy(s_kind,"question-review"); s_question_draft[0]=0; s_question_revision=0;
    const char *draft=home_string(iter,MESSAGE_KEY_QuestionDraft,65); if(draft) strcpy(s_question_draft,draft);
    strcpy(s_context_kind,"none"); s_context_id[0]=0; s_question_has_prompt=false; question_receive(iter,offer); return;
  }
  if(command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"ready-challenge")) { signal_transport_resume(&s_transport); s_ready_pending=true; flush(NULL); redraw(); return; }
  if(command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"question")) { uint32_t question_id; if(home_uint(iter,MESSAGE_KEY_RequestId,&question_id)) question_receive(iter,question_id); return; }
  if (command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"home")) {
    uint32_t home_id; if (home_uint(iter,MESSAGE_KEY_RequestId,&home_id)) home_receive(iter,home_id); return;
  }
  if (id && command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"phone-handoff")) {
    Tuple *hint=dict_find(iter,MESSAGE_KEY_StatusText);
    if (id->value->uint32==s_answer_id && s_view==VIEW_READER && hint && hint->type==TUPLE_CSTRING) {
      signal_utf8_copy(s_phone_hint,sizeof s_phone_hint,hint->value->cstring); redraw();
    }
    return;
  }
  if (id && command && command->type==TUPLE_CSTRING && !strcmp(command->value->cstring,"review")) {
    s_view=VIEW_READER; strcpy(s_status,"Review this draft on your phone. Update the companion for wrist review."); redraw(); return;
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
      s_request_id=id->value->uint32; s_phone_record=true; s_configured=true; fresh_question(false); ask(); return;
    }
    snprintf(s_kind,sizeof s_kind,"%s",command->value->cstring);
    collect(id->value->uint32); return;
  }
  if (!id || id->value->uint32!=s_request_id) { redraw(); return; }
  if (home_view()) return;
  Tuple *text=dict_find(iter,MESSAGE_KEY_ResponseText),*status=dict_find(iter,MESSAGE_KEY_StatusText);
  if (text && text->type==TUPLE_CSTRING && text->length<=901 && text->length>1) {
    if (s_answer_id==s_request_id) { s_ack_id=s_request_id; flush(NULL); return; }
    if (!busy() || s_view==VIEW_REVIEW) return;
    bool history=!strcmp(s_kind,"history");
    const char *record=home_string(iter,MESSAGE_KEY_RecordId,65), *record_kind=home_string(iter,MESSAGE_KEY_RecordKind,8);
    s_record_id[0]=s_record_kind[0]=0;
    if(!history && record && signal_home_id(record) && record_kind && (!strcmp(record_kind,"capture")||!strcmp(record_kind,"answer"))) { strcpy(s_record_id,record); strcpy(s_record_kind,record_kind); }
    signal_utf8_copy(history?s_history:s_answer,TEXT_CAP,text->value->cstring); s_answer_id=s_request_id; s_phone_hint[0]=0;
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
  signal_utf8_copy(s_prompt,sizeof s_prompt,text); s_question_has_prompt=true; question_request("question-open");
}
#endif
static void ask(void) {
  if (busy()) { cancel_turn("Stopped."); return; }
  if (!s_connected || !s_bridge_ready) { s_phone_record=false; s_view=VIEW_READER; snprintf(s_status,sizeof s_status,"Open Signal Station on your phone and check the watch connection."); redraw(); return; }
  if (!s_configured) { s_phone_record=false; s_view=VIEW_READER; snprintf(s_status,sizeof s_status,"Open Signal Station on your phone and configure a provider."); redraw(); return; }
  if (!s_question_available) { s_phone_record=false; s_view=VIEW_READER; strcpy(s_status,"Update Signal Station on your phone. Questions require its exact context review."); redraw(); return; }
#ifdef PBL_MICROPHONE
  if (!s_dictation) s_dictation=dictation_session_create(401,dictated,NULL);
  if (!s_dictation) { snprintf(s_status,sizeof s_status,"Dictation is unavailable."); s_view=VIEW_READER; redraw(); return; }
  dictation_session_enable_confirmation(s_dictation,s_confirm); dictation_session_enable_error_dialogs(s_dictation,false);
  s_view=VIEW_DICTATION; s_scroll=0; snprintf(s_status,sizeof s_status,"Speak near the watch. Back cancels.");
  if (dictation_session_start(s_dictation)!=DictationSessionStatusSuccess) cancel_turn("Dictation could not start. Check the phone.");
#else
  s_prompt[0]=0; question_request("question-open");
#endif
  redraw();
}
static void local_action(const char *kind) {
  if (!s_connected || !s_bridge_ready) {
    s_view=VIEW_READER; s_scroll=0;
    snprintf(s_status,sizeof s_status,"Open Signal Station on your phone. Capture and History work without an answer provider.");
    s_ready_pending=true; flush(NULL); redraw(); return;
  }
  request(kind,NULL);
}
static void select_click(ClickRecognizerRef r,void *context) {
  if(s_view==VIEW_WAIT && !strncmp(s_kind,"question-",9)) return;
  if(s_view==VIEW_QUESTION_DRAFT) {
    if(!s_question_draft[0] && s_question_row!=2) { question_request("question-open"); return; }
    if(s_question_row==0) question_request(s_question_draft[0]?"question-review":"question-open");
    else if(s_question_row==1) { s_question_page=0; question_request("question-systems"); }
    else if(s_question_row==2) ask();
    else question_request("question-phone");
    return;
  }
  if(s_view==VIEW_QUESTION_SYSTEMS) {
    s_question_system[0]=0;
    if(!s_question_row) { strcpy(s_question_mode,!strcmp(s_question_mode,"none")?"read":!strcmp(s_question_mode,"read")?"actions":"none"); question_request("question-home"); }
    else if(s_question_row<=s_question_systems.count) {
      SignalHomeItem *item=&s_question_systems.items[s_question_row-1]; strcpy(s_question_system,item->id); s_question_selected=strncmp(item->label,"[x] ",4)!=0; question_request("question-home");
    } else if(s_question_systems.page && s_question_row==s_question_systems.count+1) { s_question_page=s_question_systems.page-1; question_request("question-systems"); }
    else { s_question_page=s_question_systems.page+1; question_request("question-systems"); }
    return;
  }
  if(s_view==VIEW_QUESTION_REVIEW) {
    if(!s_question_read || !s_question_review[0]) return;
    if(s_question_expires<=(uint32_t)time(NULL) || s_question_expires-(uint32_t)time(NULL)>120) { clear_timeout(); question_expired(NULL); return; }
    s_question_read=false; question_request("question-send"); return;
  }
  if(s_view==VIEW_QUESTION_PHONE) { if(s_question_draft[0]) question_request("question-phone"); return; }
  if (s_view==VIEW_HOME_LIST) {
    if (s_home_selected<s_home_page.count) { memset(&s_home_intent,0,sizeof s_home_intent); strcpy(s_home_intent.favorite,s_home_page.items[s_home_selected].id); home_request("home-open"); }
    else if (s_home_page.page && s_home_selected==s_home_page.count) home_list(s_home_page.page-1);
    else if (s_home_page.page+1<s_home_page.pages) home_list(s_home_page.page+1);
    return;
  }
  if (s_view==VIEW_HOME_DETAIL) { if (s_home_intent.action[0]) home_request("home-review"); return; }
  if (s_view==VIEW_HOME_REVIEW) {
    if (!s_home_review_read) return;
    if (!signal_home_can_confirm(&s_home_intent,(uint32_t)time(NULL))) { clear_timeout(); home_expired(NULL); return; }
    s_home_intent.consumed=true; home_request("home-confirm"); return;
  }
  if (s_view==VIEW_HOME_HANDOFF) { home_request("home-phone"); return; }
  if (s_view==VIEW_HOME_RESULT) { home_list(s_home_page.page); return; }
  if (s_view==VIEW_WAIT && !strncmp(s_kind,"home-",5)) return;
  if (s_view==VIEW_REVIEW) {
    cancel_turn("Review this draft on your phone.");
  } else {
    fresh_question(s_view==VIEW_READER && !s_status[0] && s_record_id[0]); ask();
  }
}
static void select_long(ClickRecognizerRef r,void *context) {
  if (s_view==VIEW_REVIEW || home_view() || question_view()) return;
  if (s_view==VIEW_MENU) { s_view=VIEW_HELP; s_scroll=0; redraw(); }
  else if (s_view==VIEW_READER && !s_status[0] && s_answer[0] && s_answer_id) {
    if (!s_connected || !s_bridge_ready) snprintf(s_phone_hint,sizeof s_phone_hint,"Phone disconnected. Reconnect, then hold Select.");
    else { signal_transport_resume(&s_transport); s_handoff_id=s_answer_id; snprintf(s_phone_hint,sizeof s_phone_hint,"Preparing on phone..."); flush(NULL); }
    s_scroll=s_scroll_max; redraw();
  } else { fresh_question(false); ask(); }
}
static void up_click(ClickRecognizerRef r,void *context) {
  if (s_view==VIEW_QUESTION_DRAFT || s_view==VIEW_QUESTION_SYSTEMS) { if(s_question_row>0) s_question_row--; }
  else if (s_view==VIEW_HOME_LIST) { if (s_home_selected>0) s_home_selected--; }
  else if (s_view==VIEW_MENU) local_action("capture");
  else { s_scroll-=36; if (s_scroll<0) s_scroll=0; } redraw();
}
static void down_click(ClickRecognizerRef r,void *context) {
  if (s_view==VIEW_QUESTION_DRAFT || s_view==VIEW_QUESTION_SYSTEMS) { int rows=s_view==VIEW_QUESTION_DRAFT?4:1+s_question_systems.count+(s_question_systems.page>0)+(s_question_systems.page+1<s_question_systems.pages); if(s_question_row+1<rows) s_question_row++; }
  else if (s_view==VIEW_HOME_LIST) { int rows=s_home_page.count+(s_home_page.page>0)+(s_home_page.page+1<s_home_page.pages); if (s_home_selected+1<rows) s_home_selected++; }
  else if (s_view==VIEW_MENU) local_action("history");
  else { s_scroll+=36; if (s_scroll>s_scroll_max) s_scroll=s_scroll_max; if (s_scroll>=s_scroll_max) { if(s_view==VIEW_HOME_REVIEW) s_home_review_read=true; if(s_view==VIEW_QUESTION_REVIEW) s_question_read=true; } } redraw();
}
static void down_long(ClickRecognizerRef r,void *context) { if (s_view==VIEW_MENU) home_list(0); }
static void back_click(ClickRecognizerRef r,void *context) {
  if(question_view()) {
    clear_timeout(); s_question_review[0]=0; s_question_read=false;
    if(s_view==VIEW_QUESTION_SYSTEMS || s_view==VIEW_QUESTION_REVIEW) { s_view=VIEW_QUESTION_DRAFT; s_question_row=0; s_scroll=0; redraw(); return; }
    if(s_question_draft[0] && strcmp(s_kind,"question-send")) question_request("question-cancel");
    else if(s_view==VIEW_WAIT) { s_cancel_id=s_request_id; s_request_pending=false; flush(NULL); }
    s_view=VIEW_MENU; s_scroll=0; redraw(); return;
  }
  if (home_view()) {
    if (busy()) { home_cancel(); s_request_pending=false; clear_timeout(); flush(NULL); }
    s_home_intent.consumed=true;
    s_view=VIEW_MENU; s_scroll=0; redraw(); return;
  }
  if (busy()) { cancel_turn(""); s_view=VIEW_MENU; s_scroll=0; redraw(); }
  else if (s_view!=VIEW_MENU) { s_view=VIEW_MENU; s_scroll=0; redraw(); }
  else window_stack_pop(true);
}
static void clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT,select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT,650,select_long,NULL);
  window_single_repeating_click_subscribe(BUTTON_ID_UP,150,up_click);
  if (s_view==VIEW_MENU) { window_single_click_subscribe(BUTTON_ID_DOWN,down_click); window_long_click_subscribe(BUTTON_ID_DOWN,650,down_long,NULL); }
  else window_single_repeating_click_subscribe(BUTTON_ID_DOWN,150,down_click);
  window_single_click_subscribe(BUTTON_ID_BACK,back_click);
}
static GRect body_bounds(GRect b) { int inset=PBL_IF_ROUND_ELSE(b.size.w/7,7); return GRect(inset,46,b.size.w-2*inset,b.size.h-(s_view>=VIEW_HOME_LIST?94:80)); }
static GFont font(void) { return fonts_get_system_font(layer_get_bounds(s_canvas).size.h>=200 ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD); }
static int markdown_body(GContext *ctx,int width,int offset) {
  SignalMarkdown reader={.next=s_view==VIEW_HISTORY?s_history:s_answer};
  SignalMarkdownBlock block;
  static char line[TEXT_CAP];
  bool large=layer_get_bounds(s_canvas).size.h>=200;
  int y=0;
  while (signal_markdown_next(&reader,line,sizeof line,&block)) {
    if (!line[0]) {
      if (block.rule && ctx) { graphics_context_set_stroke_color(ctx,GColorWhite); graphics_draw_line(ctx,GPoint(0,y+4-offset),GPoint(width-1,y+4-offset)); }
      y+=8; continue;
    }
    GFont face=fonts_get_system_font(block.heading?(large?FONT_KEY_GOTHIC_24_BOLD:FONT_KEY_GOTHIC_18_BOLD):block.code?FONT_KEY_GOTHIC_14:(large?FONT_KEY_GOTHIC_24:FONT_KEY_GOTHIC_18));
    int inset=block.quote?8:block.code?4:block.indent;
    int height=graphics_text_layout_get_content_size(line,face,GRect(0,0,width-inset,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft).h;
    if (ctx) {
      graphics_context_set_text_color(ctx,block.heading?PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite):GColorWhite);
      if (block.quote) { graphics_context_set_stroke_color(ctx,GColorWhite); graphics_draw_line(ctx,GPoint(1,y-offset+4),GPoint(1,y-offset+height)); }
      graphics_draw_text(ctx,line,face,GRect(inset,y-offset,width-inset,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft,NULL);
    }
    y+=height+(block.heading?6:3);
  }
  if (s_view==VIEW_READER) {
    size_t length=strlen(s_answer);
    const char *note=length>=3 && !strcmp(s_answer+length-3,"…")?"Reply shortened. Hold Select: full reply on phone.":
      !strcmp(s_record_kind,"capture")?"Select: ask about this capture. Hold Select: open on phone.":!strcmp(s_record_kind,"answer")?"Select: follow up. Hold Select: full reply on phone.":"Hold Select: full reply on phone.";
    GFont small=fonts_get_system_font(FONT_KEY_GOTHIC_14);
    if (s_phone_hint[0]) note=s_phone_hint;
    y+=8;
    if (ctx) { graphics_context_set_text_color(ctx,GColorWhite); graphics_draw_text(ctx,note,small,GRect(0,y-offset,width,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft,NULL); }
    y+=graphics_text_layout_get_content_size(note,small,GRect(0,0,width,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft).h;
  }
  return y;
}
static const char *body_text(void) {
  if(s_view>=VIEW_QUESTION_DRAFT) return s_question_text;
  if (s_view==VIEW_HOME_LIST) return "Choose Home favorites on your phone.";
  if (s_view>=VIEW_HOME_DETAIL && s_view<=VIEW_HOME_HANDOFF) return s_home_text;
  if (s_view==VIEW_HELP) return "Home shortcuts\nUp: Capture\nSelect: Ask\nDown: History\nHold Down: Home favorites\n\nCapture saves selected readings on your phone without a language model request.\nHistory reads recent saved records without a provider.\nAsk uses watch dictation when available. Otherwise, ask on your phone.\n\nUp/Down scroll reports and history. Hold Select on a reply to continue on phone; tap Select to ask again. Back cancels or returns home. Hold Select here to ask.\n\nChoose sources, manage saved history, and configure providers on the phone.";
  if (s_view==VIEW_REVIEW) { snprintf(s_display,sizeof s_display,"%s\n\n%s\n\nSelect: Send\nBack: keep on phone",s_prompt,s_review_context); return s_display; }
  if (s_view==VIEW_HISTORY) return s_history;
  if (s_view==VIEW_WAIT) { snprintf(s_display,sizeof s_display,"%s%s%s",s_prompt[0]?s_prompt:"",s_prompt[0]?"\n\n":"",s_status); return s_display; }
  if (s_view==VIEW_DICTATION) return s_status;
  if (s_status[0]) { snprintf(s_display,sizeof s_display,"%s%s%s",s_status,s_answer[0]?"\n\n":"",s_answer); return s_display; }
  return s_answer[0]?s_answer:"No report yet. Back returns home.";
}
static void draw_body(Layer *layer,GContext *ctx) {
  GRect b=layer_get_bounds(layer); graphics_context_set_text_color(ctx,GColorWhite);
  if(s_view==VIEW_QUESTION_DRAFT || s_view==VIEW_QUESTION_SYSTEMS) {
    const char *mode=!strcmp(s_question_mode,"none")?"Home: None":!strcmp(s_question_mode,"read")?"Home: Read":s_view==VIEW_QUESTION_SYSTEMS?"Read + actions":"Home: actions";
    const char *draft_rows[]={s_question_draft[0]?"Review and send":"New review",mode,"Edit question","Continue on phone"};
    int rows=s_view==VIEW_QUESTION_DRAFT?4:1+s_question_systems.count+(s_question_systems.page>0)+(s_question_systems.page+1<s_question_systems.pages);
    int visible=b.size.h/32; if(visible<1) visible=1;
    int first=s_question_row>=visible?s_question_row-visible+1:0;
    for(int i=first;i<rows && i<first+visible;i++) {
      const char *label=s_view==VIEW_QUESTION_DRAFT?draft_rows[i]:i==0?mode:i<=s_question_systems.count?s_question_systems.items[i-1].label:(s_question_systems.page && i==s_question_systems.count+1)?"Previous systems":"Next systems";
      GRect row=GRect(0,(i-first)*32,b.size.w,32); bool selected=i==s_question_row;
      if(selected) { graphics_context_set_fill_color(ctx,GColorWhite); graphics_fill_rect(ctx,row,2,GCornersAll); }
      graphics_context_set_text_color(ctx,selected?GColorBlack:GColorWhite);
      graphics_draw_text(ctx,label,fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),row,GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
    }
    return;
  }
  if (s_view==VIEW_HOME_LIST) {
    if (!s_home_page.count) { graphics_draw_text(ctx,"Choose Home favorites on your phone.",font(),b,GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL); return; }
    int rows=s_home_page.count+(s_home_page.page>0)+(s_home_page.page+1<s_home_page.pages);
    int visible=b.size.h/32; if (visible<1) visible=1;
    int first=s_home_selected>=visible?s_home_selected-visible+1:0;
    for (int i=first;i<rows && i<first+visible;i++) {
      const char *label=i<s_home_page.count?s_home_page.items[i].label:(s_home_page.page && i==s_home_page.count)?"Previous page":"Next page";
      bool selected=i==s_home_selected; GRect row=GRect(0,(i-first)*32,b.size.w,32);
      if (selected) { graphics_context_set_fill_color(ctx,GColorWhite); graphics_fill_rect(ctx,row,2,GCornersAll); }
      graphics_context_set_text_color(ctx,selected?GColorBlack:GColorWhite);
      graphics_draw_text(ctx,label,fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),row,GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
    }
    return;
  }
  if (s_view==VIEW_MENU) {
    const char *keys[]={"UP","SELECT","DOWN"};
    const char *details[]={"Save readings","Speak a question","Saved records"};
    int h=b.size.h/3;
    int rail=b.size.w-54;
    GColor accent=PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite);
    GFont action_font=fonts_get_system_font(h>=40?FONT_KEY_GOTHIC_28_BOLD:FONT_KEY_GOTHIC_18_BOLD);
    GFont key_font=fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    graphics_context_set_stroke_color(ctx,accent);
    graphics_draw_line(ctx,GPoint(b.size.w-1,h/2),GPoint(b.size.w-1,2*h+h/2));
    for (int i=0;i<3;i++) {
      int cy=i*h+h/2;
      // Every row is a direct physical-button action, never a selected item.
      graphics_context_set_stroke_color(ctx,accent);
      graphics_draw_line(ctx,GPoint(rail+48,cy),GPoint(b.size.w-1,cy));
      graphics_context_set_fill_color(ctx,accent);
      graphics_fill_rect(ctx,GRect(rail,cy-9,49,19),2,GCornersAll);
      graphics_context_set_text_color(ctx,GColorBlack);
      graphics_draw_text(ctx,keys[i],key_font,GRect(rail-1,cy-12,51,20),GTextOverflowModeFill,GTextAlignmentCenter,NULL);
      graphics_context_set_text_color(ctx,GColorWhite);
      graphics_draw_text(ctx,s_items[i],action_font,GRect(0,cy-(h>=40?24:13),rail-5,h),GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
      if (h>=40) graphics_draw_text(ctx,details[i],fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(1,cy+5,rail-5,18),GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
      if (i<2) {
        graphics_context_set_stroke_color(ctx,PBL_IF_COLOR_ELSE(GColorDarkGray,GColorWhite));
        graphics_draw_line(ctx,GPoint(0,(i+1)*h-1),GPoint(rail-7,(i+1)*h-1));
      }
    }
    return;
  }
  if ((s_view==VIEW_READER && !s_status[0] && s_answer[0]) || s_view==VIEW_HISTORY) {
    markdown_body(ctx,b.size.w,s_scroll); return;
  }
  const char *text=body_text();
  graphics_draw_text(ctx,text,font(),GRect(0,-s_scroll,b.size.w,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft,NULL);
}
static void measure_body(void) {
  if(!s_body || !s_canvas) return;
  GRect b=layer_get_bounds(s_body);
  int height=((s_view==VIEW_READER && !s_status[0] && s_answer[0]) || s_view==VIEW_HISTORY)?markdown_body(NULL,b.size.w,0):
    graphics_text_layout_get_content_size(body_text(),font(),GRect(0,0,b.size.w,6000),GTextOverflowModeWordWrap,GTextAlignmentLeft).h;
  s_scroll_max=height>b.size.h?height-b.size.h:0; if(s_scroll>s_scroll_max) s_scroll=s_scroll_max;
  // Resolve layout before either layer paints. Rendering never changes consent.
  if(s_scroll_max==0) { if(s_view==VIEW_HOME_REVIEW) s_home_review_read=true; if(s_view==VIEW_QUESTION_REVIEW) s_question_read=true; }
}
static void draw(Layer *layer,GContext *ctx) {
  GRect b=layer_get_bounds(layer); int inset=PBL_IF_ROUND_ELSE(b.size.w/7,7);
  graphics_context_set_fill_color(ctx,GColorBlack); graphics_fill_rect(ctx,b,0,GCornerNone);
  graphics_context_set_text_color(ctx,PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite));
  graphics_draw_text(ctx,s_view==VIEW_QUESTION_REVIEW?"REVIEW QUESTION":s_view==VIEW_QUESTION_SYSTEMS?"QUESTION HOME":question_view()?"YOUR QUESTION":s_view==VIEW_HOME_REVIEW?"REVIEW ACTION":home_view()?"HOME FAVORITES":s_view==VIEW_MENU?"SIGNAL STATION":s_view==VIEW_HELP?"FIELD MANUAL":s_view==VIEW_REVIEW?"REVIEW DRAFT":s_view==VIEW_DICTATION?"LISTENING":s_view==VIEW_HISTORY?"RECENT HISTORY":busy()?"CONTACTING":"FIELD REPORT",fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),GRect(inset,s_view==VIEW_MENU?PBL_IF_ROUND_ELSE(10,2):9,b.size.w-inset*2,24),GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
  if (s_view==VIEW_MENU) {
    graphics_context_set_text_color(ctx,GColorWhite);
    graphics_draw_text(ctx,"PRESS RIGHT BUTTONS",fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(inset,26,b.size.w-inset*2,18),GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
  }
  graphics_context_set_stroke_color(ctx,PBL_IF_COLOR_ELSE(GColorCyan,GColorWhite)); graphics_draw_line(ctx,GPoint(inset,s_view==VIEW_MENU?44:34),GPoint(b.size.w-inset,s_view==VIEW_MENU?44:34));
  graphics_context_set_text_color(ctx,GColorWhite);
  const char *footer=s_view==VIEW_HOME_REVIEW?(s_home_review_read?"Select: Confirm | Back: cancel":"Down: read more | Back: cancel"):s_view==VIEW_HOME_LIST?"Up/Down: choose | Select":s_view==VIEW_HOME_DETAIL?(s_home_intent.action[0]?"Select: review | Back: home":"Up/Down: read | Back: home"):s_view==VIEW_HOME_HANDOFF?"Select: phone | Back: home":s_view==VIEW_HOME_RESULT?"Select: favorites | Back: home":s_view==VIEW_REVIEW?"Select: Send | Back: keep":busy()?"Back: stop":s_view==VIEW_MENU?(s_connected?(s_bridge_ready?(s_home_available?"Hold DOWN: Home":"Hold SELECT: help"):"Open phone app"):"Phone disconnected"):"Up/Down: read";
  if(s_view>=VIEW_QUESTION_DRAFT) {
    const char *primary=s_view==VIEW_QUESTION_REVIEW?(s_question_read?"Select: send":"Down: read more"):s_view==VIEW_QUESTION_PHONE?"Select: phone":s_view==VIEW_QUESTION_SYSTEMS?"Select: change":"Select: open";
    const char *secondary=s_view==VIEW_QUESTION_SYSTEMS?"Back: done":s_view==VIEW_QUESTION_REVIEW?"Back: edit":"Back: cancel";
    graphics_draw_text(ctx,primary,fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(inset,b.size.h-43,b.size.w-2*inset,18),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    graphics_draw_text(ctx,secondary,fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(inset,b.size.h-28,b.size.w-2*inset,18),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
  } else if (s_view>=VIEW_HOME_LIST) {
    const char *primary=s_view==VIEW_HOME_REVIEW?(s_home_review_read?"Select: confirm":"Down: read more"):s_view==VIEW_HOME_LIST?(s_home_page.count?"Select: open":"Choose on phone"):s_view==VIEW_HOME_DETAIL?(s_home_intent.action[0]?"Select: review":"Up/Down: read"):s_view==VIEW_HOME_HANDOFF?"Select: phone":"Select: favorites";
    const char *secondary=s_view==VIEW_HOME_REVIEW?"Back: cancel":s_view==VIEW_HOME_LIST?(s_home_page.count?"Up/Down: choose":"Back: home"):"Back: home";
    graphics_draw_text(ctx,primary,fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(inset,b.size.h-43,b.size.w-2*inset,18),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    graphics_draw_text(ctx,secondary,fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(inset,b.size.h-28,b.size.w-2*inset,18),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
  } else {
    graphics_draw_text(ctx,footer,fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(inset,b.size.h-32,b.size.w-2*inset,20),GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
  }
}
static void redraw(void) { if (s_window && s_menu_clicks!=(s_view==VIEW_MENU)) { s_menu_clicks=s_view==VIEW_MENU; window_set_click_config_provider(s_window,clicks); } if (s_body) layer_set_frame(s_body,body_bounds(layer_get_bounds(s_canvas))); measure_body(); if(s_canvas) layer_mark_dirty(s_canvas); if(s_body) layer_mark_dirty(s_body); }
static void connection_changed(bool connected) {
  s_connected=connected; if (!connected) { s_bridge_ready=false; s_home_available=s_question_available=false; }
  if (!connected && busy()) cancel_turn("Phone connection lost. Reconnect to ask again.");
  if (connected) { signal_transport_resume(&s_transport); s_ready_pending=true; flush(NULL); } redraw();
}
static void window_load(Window *window) {
  Layer *root=window_get_root_layer(window); GRect b=layer_get_bounds(root);
  s_canvas=layer_create(b); layer_set_update_proc(s_canvas,draw); layer_add_child(root,s_canvas);
  s_body=layer_create(body_bounds(b)); layer_set_update_proc(s_body,draw_body); layer_add_child(root,s_body);
}
static void window_unload(Window *window) { layer_destroy(s_body); s_body=NULL; layer_destroy(s_canvas); s_canvas=NULL; }
static void will_focus(bool in_focus) { if (!in_focus) light_enable(false); }
static void did_focus(bool in_focus) { if (in_focus) light_enable(true); }
static void init(void) {
  s_request_id=persist_exists(PERSIST_REQUEST_ID)?(uint32_t)persist_read_int(PERSIST_REQUEST_ID):(uint32_t)time(NULL);
  s_window=window_create(); window_set_background_color(s_window,GColorBlack);
  window_set_window_handlers(s_window,(WindowHandlers){.load=window_load,.unload=window_unload}); window_set_click_config_provider(s_window,clicks);
  app_message_register_inbox_received(inbox); app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent); app_message_register_outbox_failed(outbox_failed); app_message_open(2048,2048);
  s_connected=connection_service_peek_pebble_app_connection(); connection_service_subscribe((ConnectionHandlers){.pebble_app_connection_handler=connection_changed});
  app_focus_service_subscribe_handlers((AppFocusHandlers){.will_focus=will_focus,.did_focus=did_focus});
  window_stack_push(s_window,true); light_enable(true); s_ready_pending=true; flush(NULL);
}
static void deinit(void) {
  app_focus_service_unsubscribe(); light_enable(false);
  stop_sampling(); clear_timeout(); if (s_outbox_timer) app_timer_cancel(s_outbox_timer);
#ifdef PBL_MICROPHONE
  if (s_dictation) dictation_session_destroy(s_dictation);
#endif
  connection_service_unsubscribe(); app_message_deregister_callbacks(); window_destroy(s_window);
}
int main(void) { init(); app_event_loop(); deinit(); }
