from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent
subprocess.run(['python3',str(r/'build_snapshot_tests.py')],check=True)
with tempfile.TemporaryDirectory() as work:
 for name in ['snapshot_regressions','connection_regressions','protocol_test','protocol_readback_test','routing_model_test']:
  binary=Path(work)/name
  subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-pthread',str(r/(name+'.cpp')),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
