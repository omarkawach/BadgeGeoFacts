"""Rasterize Natural Earth land with the badge's actual Equal Earth code.
Usage: python tools/generate_assets.py ne_110m_land.geojson
Requires Pillow and a host C compiler. Source land data is public domain.
"""
import sys,json,math,ctypes,subprocess,tempfile
from pathlib import Path
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='geofacts-map-') as temp:
 lib=Path(temp)/'projection.so'
 subprocess.run(['cc','-shared','-fPIC',str(root/'main/projection.c'),'-lm','-o',str(lib)],check=True)
 project=ctypes.CDLL(str(lib)).world_map_point;project.argtypes=[ctypes.c_double,ctypes.c_double,ctypes.POINTER(ctypes.c_double),ctypes.POINTER(ctypes.c_double)]
 def point(lon,lat):
  x,y=ctypes.c_double(),ctypes.c_double();project(lat,lon,ctypes.byref(x),ctypes.byref(y));return x.value,y.value
 im=Image.new('1',(340,170));draw=ImageDraw.Draw(im)
 for feature in json.load(open(sys.argv[1]))['features']:
  g=feature['geometry'];polys=g['coordinates'] if g['type']=='MultiPolygon' else [g['coordinates']]
  for poly in polys:
   for ring_index,ring in enumerate(poly):
    pixels=[]
    for i,(lon,lat) in enumerate(ring):
     if i:
      prev_lon,prev_lat=ring[i-1];steps=max(1,math.ceil(max(abs(lon-prev_lon),abs(lat-prev_lat))/2))
      for k in range(1,steps):pixels.append(point(prev_lon+(lon-prev_lon)*k/steps,prev_lat+(lat-prev_lat)*k/steps))
     pixels.append(point(lon,lat))
    draw.polygon(pixels,fill=1 if ring_index==0 else 0)
 raw=im.tobytes();assert len(raw)==43*170
 (root/'main/world.h').write_text('// Equal Earth / WGS84 (EPSG:8857), 340 x 170; 43 bytes per row.\nstatic const unsigned char world_bits[] = {'+','.join(map(str,raw))+'};\n')
 (root/'previews').mkdir(exist_ok=True);im.resize((1020,510)).save(root/'previews/world-equal-earth.png')
 print('Equal Earth land bitmap regenerated with the same projection used by the badge markers.')
