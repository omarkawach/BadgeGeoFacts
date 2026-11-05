#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display.h"
#include "summary.h"
#include "epaper.h"
esp_err_t badge_epaper_setup(void){return 0;}
esp_err_t badge_epaper_start(const FC3Frame *f){return 0;}
esp_err_t badge_epaper_finish(void){return 0;}
bool badge_epaper_ready(void){return true;}
int main(int argc,char **argv){
 if(argc!=7&&argc!=8)return 2;display_init();Fact f={0};snprintf(f.title,sizeof(f.title),"%s",argv[1]);snprintf(f.text,sizeof(f.text),"%s",argv[2]);f.lat=atof(argv[3]);f.lon=atof(argv[4]);f.online=true;if(argc==8)snprintf(f.description,sizeof(f.description),"%s",argv[7]);
 if(!summarize_extract(f.title,f.text,f.summary,sizeof(f.summary))){fprintf(stderr,"No complete summary fits for %s\n",f.title);return 1;}
 printf("%s: %s\n",f.title,f.summary);display_fact(&f,atoi(argv[6])!=0);display_save(argv[5]);return 0;
}
