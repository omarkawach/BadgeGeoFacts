#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "summary.h"
#include "epaper.h"
esp_err_t badge_epaper_setup(void){return 0;}
esp_err_t badge_epaper_start(const FC3Frame *f){return 0;}
esp_err_t badge_epaper_finish(void){return 0;}
bool badge_epaper_ready(void){return true;}
static void check(const char *title,const char *input,const char *required){
 char out[768];assert(summarize_extract(title,input,out,sizeof(out)));assert(display_summary_fits(title,out));assert(out[strlen(out)-1]=='.');assert(!strstr(out,"..."));assert(strstr(out,required));printf("PASS %s: %s\n",title,out);
}
int main(void){
 check("Salar de Uyuni","Salar de Uyuni covers 10,582 square kilometres (4,086 mi2). It lies at 3,656 m (11,995 ft) above sea level. A third sentence should not be selected.","3,656 m above sea level.");
 check("Volcanic lake","The lake covers 1.5 square kilometres. It occupies a volcanic crater.","1.5 square kilometres.");
 check("Mount St. Helens","Mount St. Helens is a volcano. It lies in Washington.","Mount St. Helens is a volcano.");
 check("Potosí","Potosí lies in Bolivia. Its name contains an accented í.","Potosí");
 check("Pronunciation","A landform (pronunciation (nested)) is found in Africa. It is visible from space.","landform is found in Africa.");
 char unsupported[768];assert(!summarize_extract("Unsupported script","This alias is 東京.",unsupported,sizeof(unsupported)));
 char input[3000],out[768];memset(input,'W',sizeof(input)-2);input[sizeof(input)-2]='.';input[sizeof(input)-1]=0;assert(!summarize_extract("Overlong",input,out,sizeof(out)));
 puts("PASS: an overlong sentence is rejected, never silently clipped.");
 return 0;
}
