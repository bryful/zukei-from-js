"""SVG属性と独立したDXFリーダーでスタイル・単位換算・入力検証を確認する。"""
from pathlib import Path
import math
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tests' / '.deps'))
import ezdxf

EXE = ROOT / 'x64' / 'Release' / 'zfj.exe'
OUT = ROOT / 'tests' / 'style-output'
OUT.mkdir(exist_ok=True)
NS = {'s': 'http://www.w3.org/2000/svg'}


def run(source, ext='sjs', *args, success=True):
    """一時ソースを実行し、終了コードと出力パスを返す。"""
    path = OUT / f'case.{ext}'
    path.write_text(source, encoding='utf-8')
    result = subprocess.run([str(EXE), str(path), '--force', *args], capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stderr
    return path.with_suffix('.svg' if ext == 'sjs' else '.dxf')


def check_dxf(path):
    """通常のDXF読み込みと監査で構造を検証する。回復読み込みは使用しない。"""
    document = ezdxf.readfile(path)
    audit = document.audit()
    assert not audit.errors, audit.errors
    assert not audit.fixes, audit.fixes
    assert document.dxfversion == 'AC1018'
    assert document.units == 4
    return list(document.modelspace())


source = (ROOT / 'examples' / 'styles.sjs').read_text(encoding='utf-8-sig')
svg = ET.parse(run(source)).getroot()
shapes = list(svg.find('s:g', NS))
assert len(shapes) == 4
assert shapes[0].get('stroke') == '#0066cc'
assert shapes[0].get('stroke-width') == '0.5'
assert shapes[1].get('fill') == '#ffcc00'
assert shapes[2].get('stroke') == 'none'
assert shapes[2].get('fill') == '#00aa66'
assert shapes[3].get('stroke-width') == '0.7'

entities = check_dxf(run(source, 'djs'))
assert [e.dxftype() for e in entities] == ['LINE', 'HATCH', 'LWPOLYLINE', 'HATCH', 'HATCH', 'LWPOLYLINE']
assert entities[0].dxf.true_color == 0x0066cc
assert entities[0].dxf.lineweight == 50
assert entities[1].dxf.true_color == 0xffcc00
assert entities[1].dxf.solid_fill == 1
assert len(entities[1].paths[0].vertices) == 4
assert entities[2].dxf.true_color == 0xcc0000
assert entities[2].dxf.lineweight == 100
circle = entities[3]
assert circle.dxf.true_color == 0x00aa66
assert len(circle.paths[0].edges) == 2
assert all(edge.radius == 15 for edge in circle.paths[0].edges)
assert entities[5].dxf.lineweight == 70

for unit, dpi, factor in [('mm', 96, 1), ('pt', 300, 25.4/72), ('px', 96, 25.4/96), ('px', 300, 25.4/300)]:
    src = f'drawing.setUnit("{unit}");drawing.setDpi({dpi});drawing.rect(0,0,96,48,{{strokeWidth:2,fill:"#112233"}});'
    root = ET.parse(run(src)).getroot()
    polygon = root.find('s:g/s:polygon', NS)
    assert float(polygon.get('stroke-width')) == 2
    dxf = check_dxf(run(src, 'djs'))
    assert dxf[0].dxf.true_color == 0x112233
    assert math.isclose(dxf[0].paths[0].vertices[1][0], 96*factor)
    weights = [0,5,9,13,15,18,20,25,30,35,40,50,53,60,70,80,90,100,106,120,140,158,200,211]
    assert dxf[1].dxf.lineweight == min(weights, key=lambda w: abs(w-2*factor*100))

# 線なし・塗りなしを独立して扱い、輪郭だけを勝手に追加しない。
assert not check_dxf(run('drawing.circle(0,0,3,{stroke:null,fill:null});','djs'))
assert [e.dxftype() for e in check_dxf(run('drawing.circle(0,0,3);','djs'))] == ['CIRCLE']
# 太い線もSVGの表示範囲に収まる。
root = ET.parse(run('drawing.rect(0,0,10,10,{strokeWidth:20});')).getroot()
assert list(map(float, root.get('viewBox').split())) == [-10,-10,30,30]

for style in ['null', '[]', '{stroke:"red"}', '{fill:"#gg0000"}', '{strokeWidth:0}', '{strokeWidth:NaN}', '{strokeWidth:-1}']:
    run(f'drawing.rect(0,0,10,10,{style});', 'sjs', '--check', success=False)
run('drawing.line(0,0,1,1,{fill:"#ff0000"});', 'sjs', '--check', success=False)
run('drawing.rect(0,0,10,10,{get fill(){throw Error("getter");}});', 'sjs', '--check', success=False)
print('Style tests passed (SVG attributes, DXF audit, HATCH boundaries, units, validation).')


# 固定キャンバス: 自動余白なし、原点固定、範囲外クリップ、空図面も同じ寸法。
for unit,dpi,width_text,height_text in [('px',96,'1920px','1080px'),('px',72,'2560px','1440px'),('mm',96,'1920mm','1080mm'),('pt',96,'1920pt','1080pt')]:
    setup=f'drawing.setUnit("{unit}");drawing.setDpi({dpi});drawing.setCanvas(1920,1080);'
    for geometry in ['', 'drawing.circle(-100,2000,20,{strokeWidth:100});']:
        root=ET.parse(run(setup+geometry)).getroot()
        assert root.get('width') == width_text
        assert root.get('height') == height_text
        assert root.get('viewBox') == '0 0 1920 1080'
        assert root.get('overflow') == 'hidden'
root=ET.parse(run('drawing.setUnit("mm");drawing.setDpi(96);drawing.setCanvas(1920,1080);','sjs','--unit','px')).getroot()
assert root.get('width') == '1920px'
for invalid in ['drawing.setCanvas(0,10);','drawing.setCanvas(-1,10);','drawing.setCanvas(10,NaN);','drawing.setCanvas("10",10);','drawing.setCanvas(10);']:
    run(invalid,'sjs','--check',success=False)

# Fusion: 塗りだけの円を含めた輪郭のみ。不可視図形は出力しない。
for setup,arguments in [('drawing.setDxfMode("fusion");',()),('drawing.setDxfMode("standard");',('--fusion',))]:
    entities=check_dxf(run(setup+source+'drawing.rect(0,0,1,1,{stroke:null,fill:null});','djs',*arguments))
    assert [e.dxftype() for e in entities] == ['LINE','LWPOLYLINE','CIRCLE','LWPOLYLINE']
    assert all(not e.dxf.hasattr('true_color') and not e.dxf.hasattr('lineweight') for e in entities)
    assert all(e.dxf.color == 256 for e in entities)
    assert entities[1].closed and entities[3].closed
    assert entities[2].dxf.radius == 15
# DPI変換を保ち、キャンバス設定はDXF形状に影響しない。
entities=check_dxf(run('drawing.setUnit("px");drawing.setDpi(96);drawing.setCanvas(10,10);drawing.setDxfMode("fusion");drawing.circle(0,0,96,{stroke:null,fill:"#abcdef"});','djs'))
assert len(entities) == 1 and math.isclose(entities[0].dxf.radius,25.4)
run('drawing.line(0,0,1,1);','sjs','--fusion','--check',success=False)
run('drawing.setDxfMode("other");','djs','--check',success=False)
print('Canvas and Fusion tests passed.')
