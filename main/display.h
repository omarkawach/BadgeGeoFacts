#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "render.h"
typedef struct { char title[100],text[1800],url[256],topic[100]; double lat,lon; bool online; char summary[768]; char description[192]; } Fact;
typedef struct { char title[100],text[1800],url[256],topic[100]; double lat,lon; bool online; } LegacyFact;
typedef struct { char title[100],text[1800],url[256],topic[100]; double lat,lon; bool online; char summary[768]; } LegacySummaryFact;
void display_init(void);
void display_fact(const Fact *fact,bool wifi_connected);
void display_setup(const char *ssid,const char *password);
bool display_summary_fits(const char *title,const char *summary);
void display_refresh(void);
void display_save(const char *path);
