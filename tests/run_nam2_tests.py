from pathlib import Path
import subprocess, tempfile, os, json, struct
root = Path(__file__).resolve().parents[1]
source = root / 'source/libgp200'
with tempfile.TemporaryDirectory(prefix='gp200-nam2-') as tmp:
    # Extract unmodified complete production methods; no copied guard logic.
    midi = (source / 'MidiConnection.cpp').read_text()
    bodies = []
    for name, result in [('isFactoryAmpDestinationInactiveLocked', 'bool'),
                         ('startFactoryAmpUpload', 'bool'), ('startFactoryAmpRename', 'bool'),
                         ('processIRUpload', 'void'), ('startNamHotUpload','bool'), ('processNamHotUpload','void'), ('handleNamHotAck','bool'), ('finishNamHot','void'), ('sendParamChange', 'bool'),
                         ('buildParamChange', 'std::vector<juce::uint8>'),
                         ('nibbleEncode', 'std::vector<juce::uint8>'),
                         ('encodeDisplayValue', 'std::array<juce::uint8, 2>'),
                         ('updateCurrentPresetEffectParam', 'void'), ('updateCurrentPresetEffectId', 'void')]:
        a = midi.index('MidiConnection::' + name + ' (')
        b = midi.index('\n}', a) + 2
        bodies.append(result + ' ' + midi[a:b])
    Path(tmp, 'nam2_transport_extracted.inc').write_text('namespace gp200 {\n' + '\n'.join(bodies) + '\n}')
    # A sentinel model exercises every transpose, independently of zero-heavy DSP fixtures.
    fixture = root / 'tests/nam_hot_fixtures/compatible.nam'
    model = json.loads(fixture.read_text())
    if model['architecture']=='SlimmableContainer': model=model['config']['submodels'][0]['model']
    w=[((i*67)%199-99)*0.00001 for i in range(1871)]
    w[-1]=model['config']['head_scale']
    model['weights']=w
    fixture=Path(tmp,'sentinel.nam');fixture.write_text(json.dumps(model))
    ordered=list(w[:3]);at=3
    for k in model['config']['layers'][0]['kernel_sizes']:
        ordered += [w[at+(o*3+i)*k+t] for t in range(k) for i in range(3) for o in range(3)]
        at+=9*k;ordered+=w[at:at+6]
        ordered += [w[at+6+o*3+i] for i in range(3) for o in range(3)]
        ordered += w[at+15:at+18];at+=18
    ordered += [w[at+i*16+t] for t in range(16) for i in range(3)]
    ordered += w[at+48:]
    expected=Path(tmp,'expected.bin');expected.write_bytes(struct.pack('<1871f',*ordered))
    for test in ['nam2_test', 'nam2_transport_test', 'nam_hot_test']:
        exe = Path(tmp) / test
        subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pthread',
                        '-I' + str(root / 'tests/nam2_support'), '-I' + tmp,
                        str(root / ('tests/' + test + '.cpp')),
                        *(str(source / name) for name in ['GP200ModSync.cpp', 'GP200EffectParamDatabase.cpp',
                                                           'GP200EffectDatabase.cpp', 'GP200SoundClone.cpp', 'GP200NAM.cpp']),
                        '-o', str(exe)], check=True)
        env = dict(os.environ, TEST_NAM2_DIR=tmp)
        subprocess.run([str(exe), *([str(fixture),str(expected)] if test=='nam_hot_test' else [])], env=env, check=True)
        if test == 'nam2_test':
            subprocess.run([str(exe), '--cold-cache'], env=env, check=True)
