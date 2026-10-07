#!/usr/bin/env python3
"""Host safety tests, optionally compare every pulse with pinned upstream tables."""
import argparse
import ast
import pathlib
import re
import subprocess
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--upstream', type=pathlib.Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='papers3-epdiy-') as directory:
    binary = pathlib.Path(directory) / 'test'
    subprocess.run(['c++', '-DFREEINK_PAPERS3_EPDIY_TEST=1', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(HERE / 'test.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    subprocess.run(['c++', '-DFREEINK_PAPERS3_EPDIY_TEST=1', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I', str(HERE / 'stubs'),
                    str(HERE / 'gate_test.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
if args.upstream:
    revision = subprocess.check_output(['git', '-C', str(args.upstream), 'rev-parse', 'HEAD'], text=True).strip()
    assert revision == '42c16127dc5d4bca15e830624f22666ad914575f', revision
    source = (args.upstream / 'src/waveforms/epdiy_ED047TC1.h').read_text()
    adapted = (HERE.parent / 'PaperS3EpdiyWaveforms.h').read_text()
    for number, name in [(1, 'DIRECT_MONO'), (2, 'GRAYSCALE'), (5, 'TEXT')]:
        data = re.search(r'_' + str(number) + r'_0_data[^=]*= (.*);', source)[1]
        data = ast.literal_eval(data.replace('{', '[').replace('}', ']'))
        timing = re.search(r'_' + str(number) + r'_0_times[^=]*= \{ (.*) \};', source)[1]
        timing = [int(value) for value in timing.split(',')]
        table = re.search(name + r'\[\] = \{(.*?)\n\};', adapted, re.S)[1]
        rows = re.findall(r'\{0x([0-9a-f]+)u, (\d+), (\d+)\}', table)
        assert len(rows) == len(data)
        for i, (word, high, low) in enumerate(rows):
            assert int(high) == timing[i] and int(low) == 50
            word = int(word, 16)
            for before in range(4):
                for after in range(4):
                    old = before * 5
                    expected = (data[i][after * 5][old // 4] >> (6 - 2 * (old % 4))) & 3
                    actual = (word >> (2 * (before * 4 + after))) & 3
                    assert actual == expected, (name, i, before, after)
    print('All 1,040 extracted transitions and 65 phase timings match pinned upstream.')
print('EPDiy PaperS3 tests passed.')
