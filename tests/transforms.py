"""パスの変更・コピー、9アンカー、変換順序、楕円、SVG/DXFを検証する。"""
from pathlib import Path
import math
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / 'x64/Release/zfj.exe'
OUT = ROOT / 'tests/path-output/transform'
OUT.mkdir(parents=True, exist_ok=True)
NS = {'s': 'http://www.w3.org/2000/svg'}


def run(source, ext='sjs', *args, success=True):
    path = OUT / f'case.{ext}'
    path.write_text(source, encoding='utf-8')
    result = subprocess.run([str(EXE), str(path), '--force', *args], capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stderr
    if not success:
        return None
    if ext == 'sjs':
        return ET.parse(path.with_suffix('.svg')).getroot()
    return path.with_suffix('.dxf')


def shapes(svg):
    return list(svg.find('s:g', NS))


def close(actual, expected, tolerance=1e-9):
    assert math.isclose(float(actual), expected, abs_tol=tolerance, rel_tol=tolerance), (actual, expected)


# 変更は別名にも反映し、自分自身を返す。draw済みの図形はスナップショット。
svg = run('var a=path.rect(0,0,20,10);var alias=a;drawing.draw(a);'
          'if(path.transform(a,{pos:{x:30,y:5}})!==a)throw Error("identity");drawing.draw(alias);')
rects = shapes(svg)
assert len(rects) == 2
assert rects[0].get('points') == '0,0 20,0 20,10 0,10 '
assert rects[1].get('points') == '30,5 50,5 50,15 30,15 '

# コピーは独立し、省略は恒等変換。
svg = run('var a=path.circle(10,10,5);var b=path.transformed(a,{pos:[20,3]});'
          'if(a===b)throw Error("copy");path.transform(b,{pos:{y:7}});drawing.draw(a);drawing.draw(b);')
assert shapes(svg)[0].get('cx') == '10' and shapes(svg)[1].get('cx') == '30'
assert shapes(svg)[1].get('cy') == '20'
for params in ['', ',undefined', ',{}', ',{pos:{},scale:{},rot:0}']:
    svg = run(f'var a=path.rect(0,0,20,10);drawing.draw(path.transformed(a{params}));')
    assert shapes(svg)[0].get('points') == '0,0 20,0 20,10 0,10 '

# 9アンカーは変換前の外接矩形から決める。百分率で拡縮する。
names = ['topLeft','top','topRight','centerLeft','center','centerRight','bottomLeft','bottomCenter','bottomRight']
japanese = ['左上','上','右上','左中','中心','右中','左下','下','右下']
for i, name in enumerate(names):
    for anchor in [name, japanese[i]]:
        svg = run(f'var a=path.rect(10,20,20,10);path.transform(a,{{scale:[200,200],anchor:"{anchor}"}});drawing.draw(a);')
        points = [tuple(map(float, item.split(','))) for item in shapes(svg)[0].get('points').split()]
        close(points[0][0], 10 - 10 * (i % 3))
        close(points[0][1], 20 - 5 * (i // 3))
        close(points[2][0] - points[0][0], 40)
        close(points[2][1] - points[0][1], 20)

# キャンバス中心はパス自身の中心と異なる。設定寸法・別名を検証する。
for anchor in ['canvasCenter']:
    svg = run(f'drawing.setCanvas(200,100);var a=path.circle(20,10,5);path.transform(a,{{rot:180,anchor:"{anchor}"}});drawing.draw(a);')
    close(shapes(svg)[0].get('cx'), 180)
    close(shapes(svg)[0].get('cy'), 90)
    assert svg.get('viewBox') == '0 0 200 100'
run('path.transform(path.circle(0,0,5),{rot:30,anchor:"canvasCenter"});', success=False)
for alias, canonical in [('right','centerRight'),('top-left','topLeft')]:
    source = 'var a=path.rect(10,20,20,10);path.transform(a,{scale:[200,200],anchor:"%s"});drawing.draw(a);'
    assert ET.tostring(run(source % alias)) == ET.tostring(run(source % canonical))
for typo in ['canvusCenter', 'bottomRigt']:
    run(f'drawing.setCanvas(200,100);path.transform(path.circle(0,0,5),{{rot:30,anchor:"{typo}"}});', success=False)

# スケール→回転→移動。任意アンカーの座標も入力単位。
for anchor in ['[10,20]', '{x:10,y:20}']:
    svg = run(f'var a=path.rect(10,20,20,10);path.transform(a,{{scale:{{x:200}},rot:90,pos:[3,4],anchor:{anchor}}});drawing.draw(a);')
    points = [tuple(map(float, item.split(','))) for item in shapes(svg)[0].get('points').split()]
    close(points[0][0], 3)
    close(points[0][1], 24)
    close(points[2][0], 13)
    close(points[2][1], 64)

# 斜め回転と反転は元輪郭を維持し、線幅を拡縮しない。
svg = run('var a=path.rect(0,0,20,10,{strokeWidth:2});path.transform(a,{rot:45,scale:[-200,100]});drawing.draw(a);')
assert shapes(svg)[0].get('stroke-width') == '2'
assert len(shapes(svg)[0].get('points').split()) == 4

# 直線・任意多角形にも同じ座標変換を適用する。
svg = run('var a=path.line(0,0,10,0);path.transform(a,{scale:[200,100],rot:90,pos:[3,4],anchor:[0,0]});drawing.draw(a);'
          'var b=path.polygon([[0,0],[10,0],[0,10]]);path.transform(b,{pos:[5,7]});drawing.draw(b);')
line = shapes(svg)[0]
for key, wanted in [('x1',3),('y1',4),('x2',3),('y2',24)]:
    close(line.get(key), wanted)
assert shapes(svg)[1].get('points') == '5,7 15,7 5,17 '

# 円の等倍拡縮は円、縦横別倍率は正確な楕円。複数回変換も軸を正確に復元する。
svg = run('var a=path.circle(10,10,5);drawing.draw(path.transformed(a,{scale:[200,200],rot:33}));'
          'drawing.draw(path.transformed(a,{scale:[200,100],rot:30}));')
assert shapes(svg)[0].tag.endswith('circle')
close(shapes(svg)[0].get('r'), 10)
ellipse = shapes(svg)[1]
assert ellipse.tag.endswith('ellipse')
close(ellipse.get('rx'), 10)
close(ellipse.get('ry'), 5)
assert ellipse.get('transform') == 'rotate(30 10 10)'
source = 'var a=path.circle(0,0,5);path.transform(a,{scale:[200,100],rot:30});path.transform(a,{scale:[150,50],rot:20});drawing.draw(a);'
svg = run(source)
ellipse = shapes(svg)[0]
a, b = float(ellipse.get('rx')), float(ellipse.get('ry'))
theta = math.radians(float(ellipse.get('transform').split('(')[1].split()[0]))
actual = (a*a*math.cos(theta)**2+b*b*math.sin(theta)**2,
          (a*a-b*b)*math.sin(theta)*math.cos(theta),
          a*a*math.sin(theta)**2+b*b*math.cos(theta)**2)
u = (10*math.cos(math.radians(30)), 10*math.sin(math.radians(30)))
v = (-5*math.sin(math.radians(30)), 5*math.cos(math.radians(30)))
def matrix(p):
    c, s = math.cos(math.radians(20)), math.sin(math.radians(20))
    return (1.5*c*p[0]-.5*s*p[1], 1.5*s*p[0]+.5*c*p[1])
u, v = matrix(u), matrix(v)
expected = (u[0]**2+v[0]**2, u[0]*u[1]+v[0]*v[1], u[1]**2+v[1]**2)
for value, wanted in zip(actual, expected):
    close(value, wanted)

# SVGの自動キャンバスは回転楕円の正確な範囲に余白を付ける。
view = list(map(float, svg.get('viewBox').split()))
close(view[2], 2*math.sqrt(expected[0])+2)
close(view[3], 2*math.sqrt(expected[2])+2)

# 合成も変換できる。XORは回転長方形・楕円でも複合パスと共通線幅を保持する。
source = 'var a=path.rect(0,0,20,10);var b=path.circle(20,5,5);var v=path.svgcombine("xor",a,b);path.transform(v,{scale:[200,100],rot:30,pos:[50,20]});drawing.draw(v,{strokeWidth:2,fill:"#66aaff"});'
svg = run(source)
compound = shapes(svg)[0]
assert compound.tag.endswith('path') and compound.get('fill-rule') == 'evenodd'
assert compound.get('d').count('A') == 2 and compound.get('stroke-width') == '2'
assert not svg.findall('.//s:filter', NS)
for operation in ['and','or']:
    svg = run(source.replace('"xor"', f'"{operation}"'))
    assert svg.find('.//s:ellipse', NS) is not None
svg = run('var a=path.circle(0,0,10);path.transform(a,{scale:[200,100]});drawing.draw(path.combine("or",a,a));')
assert svg.find('s:g/s:path', NS) is not None

# 変換済みの合成でも、元オペランド・作成済みのコピーへ変更を波及させない。
svg = run('var a=path.rect(0,0,10,10);var v=path.svgcombine("or",a,a);path.transform(v,{pos:[50,0]});drawing.draw(a);drawing.draw(v,{fill:"#66aaff"});')
assert shapes(svg)[0].get('points').startswith('0,0 ')
assert svg.find('.//s:g[@data-zfj-artwork="originals"]//s:rect', NS).get('x') == '50'

# エラー時の原子性。後の子図形で座標範囲超過しても元のパスは変えない。
svg = run('var a=path.circle(0,0,5);try{path.transform(a,{scale:[1e9,1e9],pos:[1e9,0]});}catch(e){}drawing.draw(a);')
assert shapes(svg)[0].get('r') == '5'
svg = run('var a=path.rect(0,0,10,10);var b=path.rect(999999990,0,5,5);var v=path.svgcombine("or",a,b);'
          'try{path.transform(v,{scale:[200,100],anchor:[0,0]});}catch(e){}drawing.draw(v,{stroke:null,fill:"#000000"});')
originals = svg.findall('.//s:g[@data-zfj-source="original"]/s:rect', NS)
assert originals[0].get('width') == '10' and originals[1].get('x') == '999999990'
for params in ['null','123','[]','{pos:"bad"}','{rot:NaN}','{rot:Infinity}',
               '{scale:[0,100]}','{scale:[100,0]}','{scale:[100]}','{anchor:"bad"}',
               '{anchor:{x:1}}','{pos:[1,2,3]}','{pos:{x:Infinity}}']:
    run(f'path.transform(path.rect(0,0,10,10),{params});', success=False)
run('path.transform({});', success=False)
run('path.transformed();', success=False)

# DXFは独立したezdxfリーダーでELLIPSEと楕円HATCHを監査する。
sys.path.insert(0, str(ROOT/'tests/.deps'))
import ezdxf
source = 'var a=path.circle(10,20,5);path.transform(a,{scale:[200,100],rot:30,pos:[2,3]});drawing.draw(a,{fill:"#66aaff",strokeWidth:0.5});'
doc = ezdxf.readfile(run(source, 'djs'))
audit = doc.audit()
assert not audit.errors and not audit.fixes
ellipse = list(doc.modelspace().query('ELLIPSE'))[0]
close(ellipse.dxf.center.x, 12)
close(ellipse.dxf.center.y, 23)
close(ellipse.dxf.major_axis.magnitude, 10)
close(ellipse.dxf.ratio, .5)
hatch = list(doc.modelspace().query('HATCH'))[0]
assert len(hatch.paths[0].edges) == 2
assert all(edge.EDGE_TYPE == 'EllipseEdge' for edge in hatch.paths[0].edges)
for edge in hatch.paths[0].edges:
    close(edge.ratio, .5)
doc = ezdxf.readfile(run(source, 'djs', '--fusion'))
audit = doc.audit()
assert not audit.errors and not audit.fixes
assert [entity.dxftype() for entity in doc.modelspace()] == ['LWPOLYLINE']
doc = ezdxf.readfile(run(source.replace('[200,100]', '[200,200]'), 'djs'))
assert len(doc.modelspace().query('CIRCLE')) == 1

# pt/pxは変換後も最終単位で解釈し、DXFはmmへ換算する。
doc = ezdxf.readfile(run('drawing.setUnit("px");drawing.setDpi(96);'+source, 'djs'))
ellipse = list(doc.modelspace().query('ELLIPSE'))[0]
close(ellipse.dxf.major_axis.magnitude, 10*25.4/96)
print('Path transform tests passed (anchors, copies, curves, booleans, SVG/DXF audit).')
