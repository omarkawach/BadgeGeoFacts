"""Run host summary/layout regressions and render the actual C framebuffer.
Requires a C compiler and Pillow. Does not access hardware or Wi-Fi credentials.
"""
from pathlib import Path
import subprocess,tempfile,json
from PIL import Image
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='badge-display-') as tmp:
 tmp=Path(tmp)
 common=['cc','-fsanitize=address,undefined','-g','-I'+str(root/'tests/host'),'-I'+str(root/'main'),str(root/'main/display.c'),str(root/'main/summary.c'),str(root/'main/details.c'),str(root/'main/projection.c')]
 test=tmp/'summary-test';subprocess.run(common+[str(root/'tests/host/summary_test.c'),'-lm','-o',str(test)],check=True);subprocess.run([str(test)],check=True)
 details_test=tmp/'details-test';subprocess.run(common+[str(root/'tests/host/details_test.c'),'-lm','-o',str(details_test)],check=True);subprocess.run([str(details_test)],check=True)
 preview=tmp/'preview';subprocess.run(common+[str(root/'tests/host/preview.c'),'-lm','-o',str(preview)],check=True)
 out=root/'previews';out.mkdir(exist_ok=True)
 for name in ['richat','uyuni']:
  j=json.loads((root/'tests/fixtures'/f'{name}.json').read_text());c=j['coordinates'];raw=tmp/f'{name}.epaper'
  subprocess.run([str(preview),j['title'],j['extract'],str(c['lat']),str(c['lon']),str(raw),'1',j.get('description','')],check=True)
  b=raw.read_bytes();assert len(b)==96000
  im=Image.new('RGB',(800,480));palette=[(23,33,35),(255,253,244),(238,192,63),(195,44,43)];im.putdata([palette[(v>>shift)&3] for v in b for shift in [6,4,2,0]]);im.save(out/f'{name}.png')
 print('Summary regressions and framebuffer previews passed.')
