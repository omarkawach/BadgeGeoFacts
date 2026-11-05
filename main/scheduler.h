#pragma once
#include <stdbool.h>
#include <stdint.h>
#define GEO_VALID_EPOCH 1700000000LL
typedef struct {
 uint32_t interval_seconds;
 int64_t due_us,retry_after_us,last_success_epoch;
 bool has_fact,clock_restored;
} GeoSchedule;
void schedule_init(GeoSchedule *s,int64_t now_us,int64_t wall_epoch,bool has_fact,int64_t last_success_epoch,uint32_t interval_seconds);
bool schedule_sync_clock(GeoSchedule *s,int64_t now_us,int64_t wall_epoch);
bool schedule_should_fetch(const GeoSchedule *s,int64_t now_us,bool connected,bool manual_request);
void schedule_succeeded(GeoSchedule *s,int64_t now_us,int64_t wall_epoch);
void schedule_failed(GeoSchedule *s,int64_t now_us);
uint32_t schedule_remaining(const GeoSchedule *s,int64_t now_us);
int64_t schedule_legacy_last_success(int64_t saved_next_epoch,uint32_t saved_interval);
