"""PDF CLI checks; pass an Inkscape executable to also test real conversion."""
from pathlib import Path
import subprocess
import sys
import tempfile

EXE = Path(__file__).resolve().parent.parent / 'x64/Release/zfj.exe'
with tempfile.TemporaryDirectory(prefix='zfj PDF 日本語 ', dir=Path(__file__).resolve().parent) as directory:
    root = Path(directory)
    source = root / '図形.sjs'
    source.write_text('drawing.setCanvas(120, 80); drawing.circle(30, 30, 10);', encoding='utf-8')
    target = root / '出力.pdf'

    def run(*args, success=True):
        result = subprocess.run([str(EXE), str(source), *map(str, args)], capture_output=True)
        assert (result.returncode == 0) == success, result.stderr
        return result.stderr

    run('-o', target, '--check', '--inkscape', root / 'missing.exe')
    assert not target.exists()
    error = run('-o', target, '--inkscape', root / 'missing.exe', success=False)
    assert b'Cannot start Inkscape' in error
    assert not target.exists()
    target.write_bytes(b'keep existing output')
    assert b'Output exists' in run('-o', target, success=False)
    run('-o', target, '--force', '--inkscape', root / 'missing.exe', success=False)
    assert target.read_bytes() == b'keep existing output'
    run('-o', root / 'wrong.dxf', success=False)
    dxf = source.with_suffix('.djs')
    dxf.write_bytes(source.read_bytes())
    result = subprocess.run([str(EXE), str(dxf), '-o', str(target), '--force'], capture_output=True)
    assert result.returncode != 0 and b'extension' in result.stderr
    run('-o', root / 'output.svg')
    assert (root / 'output.svg').read_bytes().lstrip().startswith(b'<?xml')
    if len(sys.argv) > 1:
        run('-o', target, '--force', '--inkscape', sys.argv[1])
        assert target.read_bytes().startswith(b'%PDF-')
        print('Real Inkscape conversion passed')
print('PDF CLI checks passed')
