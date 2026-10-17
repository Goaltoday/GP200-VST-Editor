from pathlib import Path
import subprocess, tempfile, os
root = Path(__file__).resolve().parents[1]
source = root / 'source/libgp200'
with tempfile.TemporaryDirectory(prefix='gp200-nam2-') as tmp:
    # Extract unmodified complete production methods; no copied guard logic.
    midi = (source / 'MidiConnection.cpp').read_text()
    bodies = []
    for name, result in [('isFactoryAmpDestinationInactiveLocked', 'bool'),
                         ('startFactoryAmpUpload', 'bool'), ('startFactoryAmpRename', 'bool'),
                         ('processIRUpload', 'void'), ('sendParamChange', 'bool'),
                         ('buildParamChange', 'std::vector<juce::uint8>'),
                         ('nibbleEncode', 'std::vector<juce::uint8>'),
                         ('encodeDisplayValue', 'std::array<juce::uint8, 2>'),
                         ('updateCurrentPresetEffectParam', 'void'), ('updateCurrentPresetEffectId', 'void')]:
        a = midi.index('MidiConnection::' + name + ' (')
        b = midi.index('\n}', a) + 2
        bodies.append(result + ' ' + midi[a:b])
    Path(tmp, 'nam2_transport_extracted.inc').write_text('namespace gp200 {\n' + '\n'.join(bodies) + '\n}')
    for test in ['nam2_test', 'nam2_transport_test']:
        exe = Path(tmp) / test
        subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pthread',
                        '-I' + str(root / 'tests/nam2_support'), '-I' + tmp,
                        str(root / ('tests/' + test + '.cpp')),
                        *(str(source / name) for name in ['GP200ModSync.cpp', 'GP200EffectParamDatabase.cpp',
                                                           'GP200EffectDatabase.cpp', 'GP200SoundClone.cpp']),
                        '-o', str(exe)], check=True)
        env = dict(os.environ, TEST_NAM2_DIR=tmp)
        subprocess.run([str(exe)], env=env, check=True)
        if test == 'nam2_test':
            subprocess.run([str(exe), '--cold-cache'], env=env, check=True)
