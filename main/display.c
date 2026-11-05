#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "display.h"
#include "epaper.h"
#include "font.h"
#include "world.h"
#include "details.h"
#include "projection.h"
#include "config.h"
static FC3Frame frame;
#define HEADER_BG 2
#define HEADER_FG 0
static void pixel(int x,int y,int c) {
 if(x<0||x>=800||y<0||y>=480)return;
 int n=(y*800+x)/4,shift=(3-(x&3))*2;
 uint8_t *p=&frame.chunks[n/24000][n%24000];*p=(*p&~(3<<shift))|(c<<shift);
}
static void rect(int x,int y,int w,int h,int c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)pixel(i,j,c);}
static void line(int x0,int y0,int x1,int y1,int color){
 int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
 for(;;){pixel(x0,y0,color);pixel(x0+1,y0,color);if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
}
static uint32_t next_character(const char **text){
 const unsigned char *p=(const unsigned char*)*text;uint32_t cp=*p++;
 if(cp>=0xc2&&cp<0xe0&&(*p&0xc0)==0x80){cp=((cp&31)<<6)|(*p++&63);}
 else if(cp>=0xe0&&cp<0xf0&&p[0]&&p[1]&&(p[0]&0xc0)==0x80&&(p[1]&0xc0)==0x80){cp=((cp&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;}
 else if(cp>=0xf0){while((*p&0xc0)==0x80)p++;cp=0xfffd;}
 *text=(const char*)p;
 if(cp==0xa0||cp==0x202f)return ' ';
 if(cp==0x2013||cp==0x2014)return '-';
 if(cp==0x2018||cp==0x2019)return '\'';
 if(cp==0x201c||cp==0x201d)return '"';
 if(cp<32)return ' ';
 return cp;
}
static const GeoGlyph *glyph(const GeoFont *font,uint32_t cp){if(cp<32||cp>=0x250)cp='?';return &font->glyphs[cp-32];}
static int text_width(const GeoFont *font,const char *start,const char *end){int width=0;while(*start&&(!end||start<end)){width+=glyph(font,next_character(&start))->advance;}return width;}
static void text_range(int x,int y,const char *start,const char *end,const GeoFont *font,int color){
 while(*start&&(!end||start<end)){
  const GeoGlyph *g=glyph(font,next_character(&start));int rowbytes=(g->width+7)/8;
  for(int j=0;j<font->height;j++)for(int i=0;i<g->width;i++)if(font->bits[g->offset+j*rowbytes+i/8]&(128>>(i&7)))pixel(x+g->bearing+i,y+j,color);
  x+=g->advance;
 }
}
static void text(int x,int y,const char *value,const GeoFont *font,int color){text_range(x,y,value,NULL,font,color);}
// Returns the exact line count. Never adds an ellipsis or discards an overflow.
static int wrap_text(int x,int y,int width,const char *value,const GeoFont *font,int color,bool draw){
 const char *p=value;int rows=0;
 while(*p){
  while(*p==' '||*p=='\n'||*p=='\r')p++;
  if(!*p)break;
  const char *start=p,*last_space=NULL,*end=p;int used=0;
  while(*p){
   const char *before=p;if(*p=='\n'||*p=='\r')break;
   uint32_t cp=next_character(&p);int advance=glyph(font,cp)->advance;
   if(used+advance>width){p=before;break;}
   used+=advance;end=p;if(cp==' ')last_space=before;
  }
  if(*p&&*p!='\n'&&*p!='\r'&&last_space&&last_space>start){end=last_space;p=last_space+1;}
  else if(end==start){next_character(&p);end=p;}
  if(draw)text_range(x,y+rows*font->line_height,start,end,font,color);
  rows++;
 }
 return rows;
}
static const GeoFont *title_font(const char *title){return wrap_text(0,0,388,title,&geo_font_30,0,false)<=2?&geo_font_30:&geo_font_20;}
static int body_top(const char *title){const GeoFont *f=title_font(title);int rows=wrap_text(0,0,388,title,f,0,false);return 64+rows*f->line_height+20;}
bool display_summary_fits(const char *title,const char *summary){
 for(const char *p=summary;*p;){if(next_character(&p)>=0x250)return false;}
 int rows=wrap_text(0,0,388,summary,&geo_font_20,0,false);
 return rows>0&&body_top(title)+rows*geo_font_20.line_height<=476;
}
static void wifi_icon(bool connected){
 const int cx=755,cy=36;
 for(int r=7;r<=21;r+=7)for(int y=-r;y<-2;y++)for(int x=-r;x<=r;x++){
  int d=x*x+y*y;if(d>=(r-1)*(r-1)&&d<=(r+1)*(r+1)&&abs(x)<=-y+5)pixel(cx+x,cy+y,HEADER_FG);
 }
 for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++)if(x*x+y*y<=4)pixel(cx+x,cy+y,HEADER_FG);
 if(!connected){line(cx-17,14,cx+17,43,HEADER_BG);line(cx-18,14,cx+16,43,HEADER_FG);}
}
static void header(bool wifi){
 rect(0,0,800,52,HEADER_BG);text(24,10,"GeoFacts",&geo_font_20,HEADER_FG);
 char cadence[40];
 if(FACT_INTERVAL_SECONDS==3600)snprintf(cadence,sizeof(cadence),"New fact every hour");
 else snprintf(cadence,sizeof(cadence),"New fact every %uh",FACT_INTERVAL_SECONDS/3600);
 text(716-text_width(&geo_font_20,cadence,NULL),10,cadence,&geo_font_20,HEADER_FG);wifi_icon(wifi);
}
void display_init(void){for(int i=0;i<4;i++){frame.chunks[i]=malloc(24000);assert(frame.chunks[i]);}ESP_ERROR_CHECK(badge_epaper_setup());}
static void clear(void){for(int i=0;i<4;i++)memset(frame.chunks[i],0x55,24000);}
void display_setup(const char *ssid,const char *password){
 clear();header(false);text(24,82,"Connect your badge",&geo_font_30,0);
 text(24,154,"Join this Wi-Fi network:",&geo_font_20,0);text(24,195,ssid,&geo_font_30,3);
 text(24,263,"Password:",&geo_font_20,0);text(145,263,password,&geo_font_20,0);
 text(24,330,"Open http://192.168.4.1",&geo_font_20,0);
 text(24,373,"Enter your 2.4 GHz Wi-Fi details.",&geo_font_20,0);
 text(24,433,"Tap SHARE to return to facts.",&geo_font_14,0);
}
void display_fact(const Fact *f,bool wifi_connected){
 clear();header(wifi_connected);
 const GeoFont *heading=title_font(f->title);wrap_text(24,64,388,f->title,heading,0,true);
 // Summary is selected as complete sentences and checked against this exact layout.
 assert(display_summary_fits(f->title,f->summary));
 wrap_text(24,body_top(f->title),388,f->summary,&geo_font_20,0,true);
 const int ox=436,oy=64;
 for(int y=0;y<170;y++)for(int x=0;x<340;x++)pixel(ox+x,oy+y,(world_bits[y*43+x/8]&(128>>(x&7)))?0:1);
 double px,py;world_map_point(f->lat,f->lon,&px,&py);int mx=ox+lround(px),my=oy+lround(py);
 for(int dy=-8;dy<=8;dy++)for(int dx=-8;dx<=8;dx++){int d=dx*dx+dy*dy;if(d<=64)pixel(mx+dx,my+dy,d>=25?1:3);}
 // Thin neatline with a little breathing room around the map.
 rect(ox-3,oy-3,346,1,0);rect(ox-3,oy+172,346,1,0);
 rect(ox-3,oy-3,1,176,0);rect(ox+342,oy-3,1,176,0);
 // A plain, high-contrast table: one readable font, solid rules, no decoration.
 GeoDetails details;build_details(f,&details);
 for(unsigned i=0;i<details.count;i++)if(!strcmp(details.rows[i].label,"Coordinates")){
  snprintf(details.rows[i].label,sizeof(details.rows[i].label),"Lat / lon");
  snprintf(details.rows[i].value,sizeof(details.rows[i].value),"%.2f°%c / %.2f°%c",fabs(f->lat),f->lat>=0?'N':'S',fabs(f->lon),f->lon>=0?'E':'W');
 }
 const int left=433,right=778,top=248,bottom=474,split=548,value_x=558,value_width=212;
 int rows_needed[GEO_DETAIL_ROWS],minimum_height=0;
 for(;;){
  minimum_height=0;
  for(unsigned i=0;i<details.count;i++){
   rows_needed[i]=wrap_text(0,0,value_width,details.rows[i].value,&geo_font_20,0,false);
   minimum_height+=geo_font_20.height+(rows_needed[i]-1)*geo_font_20.line_height+4;
  }
  if(minimum_height<=bottom-top)break;
  int remove=-1;
  for(unsigned i=0;i<details.count;i++)if(!strcmp(details.rows[i].label,"Datum"))remove=(int)i;
  if(remove<0)for(int i=(int)details.count-1;i>=0;i--){
   const char *label=details.rows[i].label;
   if(strcmp(label,"Country")&&strcmp(label,"Region")&&strcmp(label,"Lat / lon")&&strcmp(label,"Projection")&&strcmp(label,"Source")){remove=i;break;}
  }
  assert(remove>=0);
  for(unsigned i=(unsigned)remove;i+1<details.count;i++)details.rows[i]=details.rows[i+1];
  details.count--;
 }
 int extra=(bottom-top-minimum_height)/(int)details.count;
 int remainder=(bottom-top-minimum_height)%(int)details.count;
 rect(left,top,right-left+1,1,0);rect(left,bottom,right-left+1,1,0);
 rect(left,top,1,bottom-top+1,0);rect(right,top,1,bottom-top+1,0);rect(split,top,1,bottom-top+1,0);
 int y=top;
 for(unsigned row=0;row<details.count;row++){
  int value_height=geo_font_20.height+(rows_needed[row]-1)*geo_font_20.line_height;
  int row_height=value_height+4+extra+((int)row<remainder?1:0);
  const GeoDetail *item=&details.rows[row];
  text(left+8,y+(row_height-geo_font_20.height)/2,item->label,&geo_font_20,0);
  wrap_text(value_x,y+(row_height-value_height)/2,value_width,item->value,&geo_font_20,0,true);
  y+=row_height;if(row+1<details.count)rect(left,y,right-left+1,1,0);
 }


}
void display_refresh(void){ESP_ERROR_CHECK(badge_epaper_start(&frame));ESP_ERROR_CHECK(badge_epaper_finish());}
void display_save(const char *path){FILE *f=fopen(path,"wb");if(f){for(int i=0;i<4;i++)fwrite(frame.chunks[i],1,24000,f);fclose(f);}}
