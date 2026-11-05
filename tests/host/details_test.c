#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "summary.h"
#include "details.h"
#include "epaper.h"
esp_err_t badge_epaper_setup(void){return 0;}
esp_err_t badge_epaper_start(const FC3Frame *f){return 0;}
esp_err_t badge_epaper_finish(void){return 0;}
bool badge_epaper_ready(void){return true;}
static const char *value(GeoDetails *d,const char *key){for(unsigned i=0;i<d->count;i++)if(!strcmp(d->rows[i].label,key))return d->rows[i].value;return NULL;}
int main(void){
 Fact f={.title="Salar de Uyuni",.text="Salar de Uyuni is the largest salt flat in the world, with an area of approximately 10,582 square kilometres (4,086 mi2). It is situated in southwestern Bolivia, at an elevation of 3,656 m (11,995 ft) above sea level.",.description="Salt flat in Bolivia",.lat=-20.13,.lon=-67.49};
 assert(summarize_extract(f.title,f.text,f.summary,sizeof(f.summary)));GeoDetails d;build_details(&f,&d);
 assert(d.count==7);assert(!strcmp(value(&d,"Country"),"Bolivia"));assert(!strcmp(value(&d,"Area"),"~10,582 km²"));assert(!strcmp(value(&d,"Elevation"),"3,656 m"));
 assert(!strcmp(value(&d,"Projection"),"Equal Earth"));assert(value(&d,"Coordinates"));
 puts("PASS Salar: country, original units, approximation and elevation preserved.");
 snprintf(f.title,sizeof(f.title),"Grand Canyon");snprintf(f.text,sizeof(f.text),"The Grand Canyon is in Arizona, United States. The Grand Canyon is 277 miles long, up to 18 miles wide and attains a depth of over a mile.");f.description[0]=0;
 assert(summarize_extract(f.title,f.text,f.summary,sizeof(f.summary)));build_details(&f,&d);
 assert(!strcmp(value(&d,"Country"),"United States"));assert(!strcmp(value(&d,"Length"),"277 mi"));assert(!strcmp(value(&d,"Width"),"Up to 18 mi"));assert(!value(&d,"Depth"));
 puts("PASS Canyon: length and qualified width are distinct; no guessed depth.");
 snprintf(f.text,sizeof(f.text),"A feature in Africa with no explicit measurements.");snprintf(f.summary,sizeof(f.summary),"A feature in Africa.");build_details(&f,&d);assert(!value(&d,"Area"));assert(!value(&d,"Elevation"));assert(d.count==4);
 puts("PASS missing data: unsupported rows omitted rather than invented.");
 return 0;
}
