"""正多角形・星型の半径、向き、頂点数、単位、変換・合成・DXFを検証する。"""
from pathlib import Path
import math
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / 'x64/Release/zfj.exe'
OUT = ROOT / 'tests/path-output/radial'
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


def vertices(svg):
    return [tuple(map(float, pair.split(','))) for pair in svg.find('s:g/s:polygon', NS).get('points').split()]


def close(actual, expected):
    assert math.isclose(actual, expected, abs_tol=1e-9, rel_tol=1e-9), (actual, expected)


# 作成だけでは描画しない。省略時は中心0,0、半径30、3辺・5頂点、回転0。
assert not list(run('path.regularPolygon();path.star();').find('s:g', NS))
triangle = vertices(run('drawing.draw(path.regularPolygon());'))
assert len(triangle) == 3
close(triangle[0][0], 0)
close(triangle[0][1], -30)
for x, y in triangle:
    close(math.hypot(x, y), 30)
star = vertices(run('drawing.draw(path.star());'))
assert len(star) == 10
for i, (x, y) in enumerate(star):
    close(math.hypot(x, y), 30 if i % 2 == 0 else 15)
close(star[0][0], 0)
close(star[0][1], -30)

# 頂点半径と辺中心半径。edgeモードの最初の辺は水平で上側。
for count in [3, 4, 5, 6, 12]:
    for mode in ['vertex', 'edge']:
        svg = run(f'drawing.draw(path.regularPolygon({{center:{{x:50,y:60}},radius:20,sides:{count},radiusMode:"{mode}"}}));')
        points = vertices(svg)
        assert len(points) == count
        expected = 20 if mode == 'vertex' else 20/math.cos(math.pi/count)
        for x, y in points:
            close(math.hypot(x-50, y-60), expected)
        if mode == 'edge':
            close((points[0][0]+points[1][0])/2, 50)
            close(points[0][1], 40)
            close(points[1][1], 40)
        for p, q in zip(points, points[1:]+points[:1]):
            close(math.dist(p, q), 2*expected*math.sin(math.pi/count))

# 回転は度数。90度で上向きの頂点が右へ移る。
for factory, radius in [('regularPolygon',20),('star',20)]:
    options = 'radius:20,sides:7' if factory == 'regularPolygon' else 'outerRadius:20,innerRadius:7,points:7'
    svg = run(f'drawing.draw(path.{factory}({{center:[50,60],{options},rot:90}},{{strokeWidth:2,fill:"#66aaff"}}));')
    points = vertices(svg)
    close(points[0][0], 70)
    close(points[0][1], 60)
    assert len(points) == (7 if factory == 'regularPolygon' else 14)
    node = svg.find('s:g/s:polygon', NS)
    assert node.get('stroke-width') == '2' and node.get('fill') == '#66aaff'

# 部分指定、undefined、回転の周回、入力単位とCLI上書き。
assert ET.tostring(run('drawing.draw(path.star());')) == ET.tostring(run('drawing.draw(path.star(undefined));'))
assert ET.tostring(run('drawing.draw(path.star({rot:0}));')) == ET.tostring(run('drawing.draw(path.star({rot:360}));'))
for unit in ['mm','pt','px']:
    svg = run(f'drawing.setUnit("{unit}");drawing.setDpi(96);drawing.draw(path.star());')
    for i, (x, y) in enumerate(vertices(svg)):
        close(math.hypot(x, y), 30 if i % 2 == 0 else 15)
svg = run('drawing.setUnit("px");drawing.draw(path.regularPolygon());', 'sjs', '--unit', 'mm')
assert svg.get('width').endswith('mm')
close(math.hypot(*vertices(svg)[0]), 30)

# 既存の変換・コピー・合成で使える。元は変更されない。
svg = run('var a=path.star();var b=path.transformed(a,{pos:[100,50],rot:90});drawing.draw(a);drawing.draw(b);')
nodes = svg.findall('s:g/s:polygon', NS)
assert len(nodes) == 2
assert nodes[0].get('points') != nodes[1].get('points')
close(math.hypot(*tuple(map(float,nodes[0].get('points').split()[0].split(',')))), 30)
# starの外接矩形中心は原点とは限らないため、この確認は明示アンカーで行う。
svg = run('var a=path.star();path.transform(a,{pos:[100,50],rot:90,anchor:[0,0]});drawing.draw(a);')
p = vertices(svg)[0]
close(p[0], 130)
close(p[1], 50)
for factory in ['svgcombine', 'combine']:
    svg = run(f'var a=path.regularPolygon({{sides:6}});var b=path.star({{center:[20,0]}});drawing.draw(path.{factory}("xor",a,b),{{strokeWidth:2,fill:"#66aaff"}});')
    node = svg.find('s:g/s:path', NS)
    assert node is not None and node.get('stroke-width') == '2'
    if factory == 'svgcombine':
        assert node.get('fill-rule') == 'evenodd' and node.get('d').count('M') == 2
        assert not svg.findall('.//s:filter', NS)

# 不正な半径・頂点数・モード・中心を拒否する。
for source in [
    'path.regularPolygon({sides:2});','path.regularPolygon({sides:3.5});',
    'path.regularPolygon({sides:100001});','path.regularPolygon({radius:0});',
    'path.regularPolygon({radius:-1});','path.regularPolygon({radiusMode:"bad"});',
    'path.regularPolygon({radiusMode:true});','path.regularPolygon({center:[1]});',
    'path.regularPolygon({center:[1e9,0],radius:30,sides:4,rot:90});',
    'path.star({points:2});','path.star({points:50001});','path.star({points:5.5});',
    'path.star({innerRadius:0});','path.star({innerRadius:30});',
    'path.star({outerRadius:10});','path.star({outerRadius:Infinity});',
    'path.star({rot:NaN});','path.star(null);','path.regularPolygon([]);',
    'path.star({}, {strokeWidth:0});'
]:
    run(source, success=False)

# DXFの閉じた輪郭・塗りを独立したリーダーで監査する。
sys.path.insert(0, str(ROOT/'tests/.deps'))
import ezdxf
source = 'drawing.draw(path.regularPolygon({center:[50,50],sides:6,radiusMode:"edge"}),{fill:"#66aaff"});drawing.draw(path.star({center:[130,50]}),{fill:"#ffcc66"});'
for args in [(), ('--fusion',)]:
    doc = ezdxf.readfile(run(source, 'djs', *args))
    audit = doc.audit()
    assert not audit.errors and not audit.fixes
    polylines = list(doc.modelspace().query('LWPOLYLINE'))
    assert [len(p) for p in polylines] == [6,10] and all(p.closed for p in polylines)
    assert len(doc.modelspace().query('HATCH')) == (0 if args else 2)
doc = ezdxf.readfile(run('drawing.setUnit("px");drawing.setDpi(96);drawing.draw(path.star());','djs'))
first = list(doc.modelspace().query('LWPOLYLINE'))[0][0]
close(first[1], -30*25.4/96)
print('Radial shape tests passed (radii, rotation, defaults, units, transforms, SVG/DXF).')

# 直接描画APIはpath生成+drawと同一のSVG/DXFを出力する。
for function in ['regularPolygon', 'star']:
    options = '{center:[50,60],radius:20,sides:6,radiusMode:"edge",rot:15}' if function == 'regularPolygon' else '{center:[50,60],outerRadius:20,innerRadius:8,points:7,rot:15}'
    for arguments in ['', '{}', 'undefined', options, options+',{stroke:"#123456",fill:"#66aaff",strokeWidth:2}']:
        direct = f'drawing.{function}({arguments});'
        indirect = f'drawing.draw(path.{function}({arguments}));'
        assert ET.tostring(run(direct)) == ET.tostring(run(indirect))
        expected = run(indirect, 'djs').read_text()
        assert run(direct, 'djs').read_text() == expected
        expected = run(indirect, 'djs', '--fusion').read_text()
        assert run(direct, 'djs', '--fusion').read_text() == expected
    # 直接描画の戻り値は従来のdrawing関数と同じundefined。
    run(f'if(drawing.{function}()!==undefined)throw Error("return value");')
    for arguments in ['null','{rot:NaN}','{center:[1]}','{}, {strokeWidth:0}']:
        run(f'drawing.{function}({arguments});', success=False)
    svg = run(f'try{{drawing.{function}({{}},{{strokeWidth:0}});}}catch(e){{}}')
    assert not list(svg.find('s:g', NS))
svg = run('drawing.combine("or",()=>{drawing.regularPolygon();drawing.star();});')
assert svg.find('s:g/s:path', NS) is not None
print('Direct radial drawing tests passed (SVG/DXF equivalence, validation, combine).')
