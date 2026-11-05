"""Check the firmware's Equal Earth transform against PROJ EPSG:8857.
Requires pyproj and a host C compiler. No hardware/network access.
"""
import ctypes,subprocess,tempfile,math
from pathlib import Path
from pyproj import Transformer
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='geofacts-projection-') as temp:
 lib=Path(temp)/'projection.so';subprocess.run(['cc','-shared','-fPIC',str(root/'main/projection.c'),'-lm','-o',str(lib)],check=True)
 dll=ctypes.CDLL(str(lib));forward=dll.equal_earth_wgs84;pixels=dll.world_map_point
 for fn in [forward,pixels]:fn.argtypes=[ctypes.c_double,ctypes.c_double,ctypes.POINTER(ctypes.c_double),ctypes.POINTER(ctypes.c_double)]
 reference=Transformer.from_crs('EPSG:4326','EPSG:8857',always_xy=True)
 maximum=0;count=0
 for lat in [-90,-89,-80,-60,-30,0,21.11,30,53.5,60,75,89,90]:
  for lon in [-180,-179,-120,-67.49,-11.39,0,30,108,179,180]:
   x,y=ctypes.c_double(),ctypes.c_double();forward(lat,lon,ctypes.byref(x),ctypes.byref(y));rx,ry=reference.transform(lon,lat)
   error=math.hypot(x.value-rx,y.value-ry);maximum=max(maximum,error);assert error<0.001,(lat,lon,error)
   pixels(lat,lon,ctypes.byref(x),ctypes.byref(y));assert -1e-8<=x.value<=339+1e-8 and -1e-8<=y.value<=169+1e-8;count+=1
 print(f'PASS: {count} points, including poles/dateline, agree with PROJ EPSG:8857 to <1 mm; max {maximum:.9f} m.')
