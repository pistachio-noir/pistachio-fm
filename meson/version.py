import pathlib
import subprocess
import sys

source, output, major = sys.argv[1:]
try:
    commit = subprocess.check_output(['git', '-C', source, 'rev-parse', '--short', 'HEAD'],
                                     stderr=subprocess.DEVNULL, text=True).strip()
except (OSError, subprocess.CalledProcessError):
    commit = ''
version = major + (' · ' + commit if commit else '')
content = f'#define PFM_VERSION "{version}"\n'
path = pathlib.Path(output)
if not path.exists() or path.read_text() != content:
    path.write_text(content)
