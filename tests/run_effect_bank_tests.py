from pathlib import Path
import subprocess, tempfile, os, hashlib
r = Path(__file__).resolve().parents[1]
src = r / 'source/libgp200'
with tempfile.TemporaryDirectory(prefix='effect-bank-') as tmp:
    exe = Path(tmp) / 'bank'
    editor=(r/'source/GP200Plugin/EffectBlockComponent.cpp').read_text()
    bodies=[]
    for name in ['getEffectName', 'getEffectDisplayName']:
        a=editor.index('EffectBlockComponent::'+name+' (');b=editor.index('\n}',a)+2
        bodies.append('juce::String '+editor[a:b])
    for name in ['bool usesDelayTimeControls (', 'bool isDelayTimeParameter (',
                 'int findDelayTimeSyncParamIndex (', 'bool isDelayTimeSyncEnabled (']:
        a=editor.index(name);b=editor.index('\n}',a)+2
        bodies.append(editor[a:b])
    a=editor.index('EffectBlockComponent::setParameterValueForDisplay (');b=editor.index('\n}',a)+2
    bodies.append('void '+editor[a:b])
    Path(tmp,'effect_bank_ui_extracted.inc').write_text('\n'.join(bodies))
    midi=(src/'MidiConnection.cpp').read_text();midi_bodies=[]
    for name,ret in [('sendParamChange','bool'),('updateCurrentPresetEffectId','void'),
                     ('updateCurrentPresetEffectParam','void'),('buildParamChange','std::vector<juce::uint8>'),
                     ('encodeDisplayValue','std::array<juce::uint8, 2>'),('nibbleEncode','std::vector<juce::uint8>')]:
        a=midi.index('MidiConnection::'+name+' (');b=midi.index('\n}',a)+2
        midi_bodies.append(ret+' '+midi[a:b])
    Path(tmp,'effect_bank_midi_extracted.inc').write_text('namespace gp200 {\n'+'\n'.join(midi_bodies)+'\n}')
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-pthread',
                    '-I'+str(r/'tests/nam2_support'),'-I'+tmp,str(r/'tests/effect_bank_test.cpp'),
                    *(str(src/n) for n in ['GP200ModSync.cpp','GP200EffectDatabase.cpp','GP200EffectParamDatabase.cpp']),
                    '-o',str(exe)],check=True)
    subprocess.run([str(exe),str(r/'tests/effect_bank_fixtures')],env=dict(os.environ,TEST_NAM2_DIR=tmp),check=True)
    subprocess.run([str(exe),str(r/'tests/effect_bank_fixtures'),'--cold-cache'],env=dict(os.environ,TEST_NAM2_DIR=tmp),check=True)
# Verify the audio callback and all MIDI serializers remain unchanged from 0.1.8.
def method(text, needle):
    a=text.index(needle); b=text.index('\n}',a)+2
    return text[a:b].encode()
import json
expected=json.loads((r/'tests/effect_bank_fixtures/unchanged_paths.json').read_text())
for name,check in expected.items():
    actual=hashlib.sha256(method((r/check['file']).read_text(),check['method'])).hexdigest()
    assert actual==check['sha256'],name
print('Audio callback and MIDI parameter/effect serializers unchanged (SHA-256 checks).')
