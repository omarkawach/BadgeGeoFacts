#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "details.h"
#include "summary.h"
static bool word(const char *text,const char *name){
 size_t n=strlen(name);for(const char *p=text;(p=strstr(p,name));p++)if((p==text||!isalpha((unsigned char)p[-1]))&&!isalpha((unsigned char)p[n]))return true;return false;
}
static void add(GeoDetails *d,const char *key,const char *value){if(d->count>=GEO_DETAIL_ROWS||!value[0])return;GeoDetail *r=&d->rows[d->count++];snprintf(r->label,sizeof(r->label),"%s",key);snprintf(r->value,sizeof(r->value),"%s",value);}
static const char *country(const char *text){
 static const char *names[]={"United States","United Kingdom","New Zealand","South Africa","Saudi Arabia","South Korea","North Korea","United Arab Emirates","Democratic Republic of the Congo","Republic of the Congo","Central African Republic","Papua New Guinea","Equatorial Guinea","Costa Rica","Czech Republic","Czechia","Sri Lanka","Burkina Faso","North Macedonia","South Sudan","Bolivia","Botswana","Mauritania","Kazakhstan","Uzbekistan","Russia","China","Australia","Peru","Italy","Egypt","India","Bangladesh","Jordan","Israel","Palestine","Antarctica","Ethiopia","Tanzania","Argentina","Brazil","Chad","Canada","Japan","Iceland","Nepal","Malawi","Yemen","Ecuador","Spain","Morocco","Norway","Chile","Indonesia","Panama","Nigeria","Namibia","France","Switzerland","Uganda","Zambia","Zimbabwe","Mexico","Greenland","Denmark","Germany","Kenya","Angola","Turkey","Türkiye","Greece","Algeria","Pakistan","Iran","Portugal","Finland","Sweden","Madagascar","Malaysia","Thailand","Vietnam","Mongolia","Colombia","Venezuela","Sudan","Ghana","Senegal"};
 const char *found=NULL;unsigned count=0;
 for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)if(word(text,names[i])){found=names[i];count++;}
 return count==1?found:NULL;
}
static void location(const Fact *f,GeoDetails *d){
 const char *c=country(f->description);if(!c)c=country(f->summary);
 if(c){add(d,!strcmp(c,"Antarctica")?"Region":"Country",c);return;}
 static const char *regions[]={"South America","North America","Central America","Atlantic Ocean","Pacific Ocean","Indian Ocean","Arctic Ocean","Southern Ocean","Sahara","Siberia","Antarctica","Europe","Africa","Asia","Caribbean","Middle East","Mediterranean"};
 for(unsigned i=0;i<sizeof(regions)/sizeof(regions[0]);i++)if(word(f->description,regions[i])){add(d,"Region",regions[i]);return;}
}
static bool nearby(const char *text,const char *word_value){return strstr(text,word_value)!=NULL;}
static const char *preceding_measure(const char *before){
 struct Term{const char *word,*label;};static const struct Term terms[]={{"elevation","Elevation"},{"altitude","Elevation"},{"depth","Depth"},{"diameter","Diameter"},{"height","Height"},{"length","Length"},{"width","Width"},{"prominence",NULL},{"volume",NULL},{"distance",NULL}};
 const char *latest=NULL,*label=NULL;size_t length=strlen(before);
 for(unsigned i=0;i<sizeof(terms)/sizeof(terms[0]);i++)for(const char *p=before;(p=strstr(p,terms[i].word));p++){
  if(length-(size_t)(p-before)<=40&&(!latest||p>latest)){latest=p;label=terms[i].label;}
 }
 return label;
}
static bool ends(const char *text,const char *suffix){size_t n=strlen(text),m=strlen(suffix);return n>=m&&!strcmp(text+n-m,suffix);}
static void measurements(const Fact *f,GeoDetails *d){
 char text[2048];clean_fact_text(f->text,text,sizeof(text));unsigned added=0;
 struct Unit{const char *source,*value;bool area;};
 static const struct Unit units[]={
 {"square kilometres","km²",true},{"square kilometers","km²",true},{"square kilometre","km²",true},{"square kilometer","km²",true},{"square miles","mi²",true},{"square metres","m²",true},{"square meters","m²",true},{"sq km","km²",true},{"km²","km²",true},{"km2","km²",true},{"km^2","km²",true},{"m²","m²",true},{"m2","m²",true},
 {"kilometres","km",false},{"kilometers","km",false},{"kilometre","km",false},{"kilometer","km",false},{"metres","m",false},{"meters","m",false},{"metre","m",false},{"meter","m",false},{"miles","mi",false},{"mile","mi",false},{"feet","ft",false},{"km","km",false},{"ft","ft",false},{"m","m",false}};
 for(const char *p=text;*p&&added<2;p++){
  if(!isdigit((unsigned char)*p))continue;
  if(p>text&&(isalnum((unsigned char)p[-1])||p[-1]=='.'||p[-1]==','||p[-1]=='-'||p[-1]=='^'))continue;
  const char *previous=p;while(previous>text&&previous[-1]==' ')previous--;if(previous<p&&previous>text&&isdigit((unsigned char)previous[-1]))continue;
  const char *end=p;while(isdigit((unsigned char)*end)||*end==','||*end=='.'||(*end=='-'&&isdigit((unsigned char)end[1])))end++;
  if(end>p&&end[-1]=='.'){end--;}
  size_t len=(size_t)(end-p);if(!len||len>28)continue;
  const char *u=end;while(*u==' ')u++;
  const struct Unit *unit=NULL;size_t unit_len=0;
  for(unsigned i=0;i<sizeof(units)/sizeof(units[0]);i++){
   size_t n=strlen(units[i].source);if(!strncmp(u,units[i].source,n)&&!isalnum((unsigned char)u[n])&&(unsigned char)u[n]<128){unit=&units[i];unit_len=n;break;}
  }
  if(!unit)continue;
  char before[81],after[81];size_t n=(size_t)(p-text);if(n>80)n=80;memcpy(before,p-n,n);before[n]=0;snprintf(after,sizeof(after),"%.80s",u+unit_len);
  for(char *q=before;*q;q++){*q=(char)tolower((unsigned char)*q);}
  for(char *q=after;*q;q++){*q=(char)tolower((unsigned char)*q);}
  const char *label=NULL;
  if(unit->area)label="Area";
  else if(nearby(after,"wide")&&strstr(after,"wide")-after<12)label="Width";
  else if(nearby(after,"long")&&strstr(after,"long")-after<12)label="Length";
  else if(nearby(after,"deep")&&strstr(after,"deep")-after<12)label="Depth";
  else if(nearby(after,"above sea level")&&strstr(after,"above sea level")-after<12)label="Elevation";
  else if((nearby(after,"high")&&strstr(after,"high")-after<12)||(nearby(after,"tall")&&strstr(after,"tall")-after<12))label="Height";
  else label=preceding_measure(before);
  if(!label)continue;
  bool duplicate=false;for(unsigned i=0;i<d->count;i++)if(!strcmp(d->rows[i].label,label))duplicate=true;if(duplicate)continue;
  // Keep source qualifiers and units. Never reinterpret a range endpoint as an exact value.
  if((n>=3&&!strcmp(before+n-3,"to ")&&!(n>=6&&!strcmp(before+n-6,"up to ")))||(n>=2&&before[n-2]=='-'))continue;
  const char *prefix="";
  if(ends(before,"approximately ")||ends(before,"about ")||ends(before,"roughly ")||ends(before,"around "))prefix="~";
  else if(ends(before,"up to "))prefix="Up to ";
  else if(ends(before,"more than ")||ends(before,"over "))prefix=">";
  else if(ends(before,"less than "))prefix="<";
  else if(ends(before,"at least "))prefix=">=";
  else if(ends(before,"at most "))prefix="<=";
  char number[32],value[96];memcpy(number,p,len);number[len]=0;
  snprintf(value,sizeof(value),"%s%s %s",prefix,number,unit->value);add(d,label,value);added++;p=u+unit_len-1;
 }
}
void build_details(const Fact *f,GeoDetails *d){
 memset(d,0,sizeof(*d));location(f,d);measurements(f,d);
 char coords[96];snprintf(coords,sizeof(coords),"%.2f° %c / %.2f° %c",fabs(f->lat),f->lat>=0?'N':'S',fabs(f->lon),f->lon>=0?'E':'W');add(d,"Coordinates",coords);
 add(d,"Projection","Equal Earth");add(d,"Datum","WGS84");add(d,"Source","Wikipedia");
}
