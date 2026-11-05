"""Test scheduling with simulated time; no Wi-Fi, hardware, or waiting required."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='geofacts-schedule-') as tmp:
 target=Path(tmp)/'scheduler-test'
 subprocess.run(['cc','-fsanitize=address,undefined','-g','-I'+str(root/'main'),str(root/'main/scheduler.c'),str(root/'tests/host/scheduler_test.c'),'-o',str(target)],check=True)
 subprocess.run([str(target)],check=True)
