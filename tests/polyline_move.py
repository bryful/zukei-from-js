"""開いた折れ線と相対移動の変更・コピー・原子性・SVG/DXFを検証する。"""
from pathlib import Path
import math
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT/'x64/Release/zfj.exe'
OUT = ROOT/'tests/path-output/polyline-move'
OUT.mkdir(parents=True, exist_ok=True)
NS = {'s':'http://www.w3.org/2000/svg'}


def run(source, ext='sjs', *args, success=True):
    path = OUT/f'case.{ext}'
    path.write_text(source, encoding='utf-8')
    result = subprocess.run([str(EXE), str(path), '--force', *args], capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stderr
    if not success:
        return None
    return ET.parse(path.with_suffix('.svg')).getroot() if ext == 'sjs' else path.with_suffix('.dxf')


def shapes(svg):
    return list(svg.find('s:g', NS))


# 同じ頂点でもpolygonだけが閉じる。直接描画とpath生成は同じ出力。
source = 'drawing.polyline([[10,10],[40,10],[40,40]],{stroke:"#003366",strokeWidth:2});'
svg = run(source)
node = shapes(svg)[0]
assert node.tag.endswith('polyline') and node.get('fill') == 'none'
assert node.get('stroke-width') == '2'
assert ET.tostring(svg) == ET.tostring(run('drawing.draw(path.polyline([[10,10],[40,10],[40,40]],{stroke:"#003366",strokeWidth:2}));'))
assert shapes(run('drawing.polygon([[10,10],[40,10],[40,40]]);'))[0].tag.endswith('polygon')
assert not shapes(run('path.polyline([[0,0],[10,10]]);'))
assert shapes(run('drawing.polyline([[0,0],[10,10]]);'))[0].get('points') == '0,0 10,10 '

# moveは同一オブジェクトを返し、movedは独立コピー。draw済みの図形は変わらない。
svg = run('var a=path.polyline([[0,0],[10,0],[10,10]]);drawing.draw(a);'
          'var alias=a;if(a.move(20,5)!==a)throw Error("identity");drawing.draw(alias);'
          'var b=a.moved(0,20);if(a===b)throw Error("copy");drawing.draw(a);drawing.draw(b);')
assert [node.get('points') for node in shapes(svg)] == [
    '0,0 10,0 10,10 ', '20,5 30,5 30,15 ', '20,5 30,5 30,15 ', '20,25 30,25 30,35 ']

# namespace形式、メソッド形式、コピーのメソッド連結は同じ相対移動。
one = 'var a=path.circle(10,20,5);a.move(3,4);drawing.draw(a.moved(7,6));'
two = 'var a=path.circle(10,20,5);path.move(a,3,4);drawing.draw(path.moved(a,7,6));'
assert ET.tostring(run(one)) == ET.tostring(run(two))
svg = run('drawing.draw(path.circle(10,20,5).moved(3,4).move(7,6));')
assert shapes(svg)[0].get('cx') == '20' and shapes(svg)[0].get('cy') == '30'

# 合成・星型・楕円も移動でき、幾何種別と線幅は維持する。
for expression in ['path.rect(0,0,10,10)', 'path.star()', 'path.regularPolygon()',
                   'path.transformed(path.circle(0,0,5),{scale:[200,100],rot:30})',
                   'path.svgcombine("xor",path.rect(0,0,10,10),path.circle(10,5,5))',
                   'path.combine("or",path.rect(0,0,10,10),path.circle(10,5,5))']:
    direct = f'var a={expression};drawing.draw(a.moved(30,40),{{strokeWidth:2}});'
    generic = f'var a={expression};drawing.draw(path.transformed(a,{{pos:[30,40]}}),{{strokeWidth:2}});'
    # 楕円軸の再計算に微小な誤差が出るため、形状タグ・線幅を比較する。
    if 'scale:' in expression:
        svg = run(direct)
        node = shapes(svg)[0]
        assert node.tag.endswith('ellipse') and node.get('cx') == '30' and node.get('cy') == '40'
        assert node.get('rx') == '10' and node.get('ry') == '5' and node.get('stroke-width') == '2'
    else:
        assert ET.tostring(run(direct)) == ET.tostring(run(generic))

# 折れ線にも既存の回転・拡縮が適用される。
svg = run('var a=path.polyline([[0,0],[10,0]]);path.transform(a,{rot:90,scale:[200,100],anchor:[0,0]});drawing.draw(a);')
points = [tuple(map(float,item.split(','))) for item in shapes(svg)[0].get('points').split()]
assert math.isclose(points[1][0], 0, abs_tol=1e-9) and math.isclose(points[1][1],20)

# 開いた図形の塗り・領域合成を拒否する。失敗時は描画・元パスを変更しない。
for source in [
    'drawing.polyline([]);','path.polyline([[0,0]]);','path.polyline([[0,0],[NaN,1]]);',
    'drawing.polyline([[0,0],[1,1]],{fill:"#ff0000"});',
    'drawing.draw(path.polyline([[0,0],[1,1]]),{fill:"#ff0000"});',
    'path.svgcombine("or",path.polyline([[0,0],[1,1]]),path.rect(0,0,1,1));',
    'path.combine("xor",path.polyline([[0,0],[1,1]]),path.rect(0,0,1,1));',
    'drawing.combine("or",()=>{drawing.polyline([[0,0],[1,1]]);drawing.rect(0,0,1,1);});',
    'path.circle(0,0,5).move(NaN,1);','path.circle(0,0,5).moved(1);',
    'path.move({},1,2);','path.moved(path.circle(0,0,5),1,Infinity);',
    'var a=path.circle(0,0,5);a.move.call({},1,2);'
]:
    run(source, success=False)
svg = run('var a=path.polyline([[0,0],[10,10]]);try{a.move(1e9,0);}catch(e){}drawing.draw(a);')
assert shapes(svg)[0].get('points') == '0,0 10,10 '

# DXFは閉じフラグ0のLWPOLYLINE。polygonは従来の閉じフラグを維持。
sys.path.insert(0, str(ROOT/'tests/.deps'))
import ezdxf
source = 'drawing.polyline([[0,0],[10,0],[10,10]],{strokeWidth:0.5});drawing.polygon([[20,0],[30,0],[30,10]]);'
for args in [(), ('--fusion',)]:
    doc = ezdxf.readfile(run(source, 'djs', *args))
    audit = doc.audit()
    assert not audit.errors and not audit.fixes
    polylines = list(doc.modelspace().query('LWPOLYLINE'))
    assert len(polylines) == 2 and not polylines[0].closed and polylines[1].closed
    assert not doc.modelspace().query('HATCH')
source = 'drawing.setUnit("px");drawing.setDpi(96);drawing.polyline([[0,0],[96,0]]);'
doc = ezdxf.readfile(run(source, 'djs'))
assert math.isclose(list(doc.modelspace().query('LWPOLYLINE'))[0][1][0], 25.4)
print('Polyline/move tests passed (open contours, copies, transforms, validation, SVG/DXF).')
