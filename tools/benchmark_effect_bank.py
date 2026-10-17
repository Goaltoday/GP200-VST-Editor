"""Host-only UI metadata benchmark. Optional baseline is a source checkout path.
This does not measure GP200 DSP, GUI frame times or a linked JUCE build.
"""
from pathlib import Path
import subprocess,tempfile,os,json,sys,shutil
root=Path(__file__).resolve().parents[1]
checkouts=[('current',root)]
if len(sys.argv)>1:checkouts.insert(0,('baseline',Path(sys.argv[1]).resolve()))
result={}
for tag,checkout in checkouts:
    with tempfile.TemporaryDirectory(prefix='bank-benchmark-') as temp:
        tmp=Path(temp)
        (tmp/'GP200').mkdir()
        shutil.copyfile(root/'tests/effect_bank_fixtures/four_blocks.json',tmp/'GP200/GP200_EFFECT_BANK.json')
        exe=tmp/'benchmark'
        # Resolve production headers relative to the chosen checkout.
        source=(root/'tests/effect_bank_benchmark.cpp').read_text().replace('../source/libgp200/',str(checkout/'source/libgp200')+'/')
        bench=tmp/'benchmark.cpp';bench.write_text(source)
        subprocess.run(['g++','-std=c++20','-O2','-pthread','-I'+str(root/'tests/nam2_support'),str(bench),
            *(str(checkout/'source/libgp200'/name) for name in ['GP200ModSync.cpp','GP200EffectDatabase.cpp','GP200EffectParamDatabase.cpp']),'-o',str(exe)],check=True)
        value=subprocess.run([str(exe)],env=dict(os.environ,TEST_NAM2_DIR=temp),check=True,capture_output=True,text=True)
        result[tag]=json.loads(value.stdout)
result['scope']='Host, modeled JUCE/JSON/filesystem; same metadata-query workload, baseline has no generic bank support. No hardware DSP claim.'
print(json.dumps(result,indent=2))
