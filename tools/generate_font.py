from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import math
root=Path(__file__).resolve().parents[1]
(root/'assets/fonts').mkdir(parents=True,exist_ok=True)
fontpath=root/'assets/fonts/IBMPlexSans.ttf'
if not fontpath.exists():raise SystemExit('Font download incomplete')
points=list(range(32,0x250))
lines=['// Generated from IBM Plex Sans, SIL Open Font License 1.1.','// Variable glyph advances; full glyph bounds prevent clipped W/w.','#include <stdint.h>','typedef struct { uint32_t offset; uint8_t width,advance; int8_t bearing; } GeoGlyph;','typedef struct { const GeoGlyph *glyphs; const uint8_t *bits; uint8_t height,line_height; } GeoFont;']
for size in [14,20,30]:
 font=ImageFont.truetype(str(fontpath),size)
 # Set the variable font to medium weight for crisp 1-bit display text.
 try:font.set_variation_by_axes([500,100])
 except Exception:pass
 ascent,descent=font.getmetrics();height=ascent+descent;data=[];glyphs=[]
 for cp in points:
  c=chr(cp);left,top,right,bottom=font.getbbox(c,anchor='ls');width=max(1,right-left)
  im=Image.new('L',(width,height));ImageDraw.Draw(im).text((-left,ascent),c,font=font,fill=255,anchor='ls')
  off=len(data);rowbytes=(width+7)//8
  for y in range(height):
   for b in range(rowbytes):
    value=0
    for bit in range(8):
     x=b*8+bit
     if x<width and im.getpixel((x,y))>=96:value|=128>>bit
    data.append(value)
  glyphs.append((off,width,max(1,round(font.getlength(c))),left))
 lines+=['static const uint8_t geo_bits_'+str(size)+'[]={'+','.join(map(str,data))+'};','static const GeoGlyph geo_glyphs_'+str(size)+'[]={'+','.join('{'+','.join(map(str,g))+'}' for g in glyphs)+'};','static const GeoFont geo_font_'+str(size)+'={geo_glyphs_'+str(size)+',geo_bits_'+str(size)+','+str(height)+','+str(height+2)+'};']
 print('Font',size,'height',height,'bytes',len(data))
(root/'main/font.h').write_text('\n'.join(lines)+'\n')
