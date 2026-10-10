"""円の互換性、楕円・円弧、変換端点、SVG/DXF/Fusionを検証する。"""
from pathlib import Path
import math
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tests/.deps'))
import ezdxf
EXE = ROOT / 'x64/Release/zfj.exe'
OUT = ROOT / 'tests/path-output/circle-arcs'
OUT.mkdir(parents=True, exist_ok=True)
NS = {'s': 'http://www.w3.org/2000/svg'}

def run(code, ext='sjs', *args, success=True):
    source = OUT / f'case.{ext}'
    source.write_text(code, encoding='utf-8')
    p = subprocess.run([str(EXE), str(source), '--force', *args], capture_output=True, text=True)
    assert (p.returncode == 0) == success, (code, p.stderr)
    if not success:
        return p.stderr
    if ext == 'sjs':
        return ET.parse(source.with_suffix('.svg')).getroot()
    doc = ezdxf.readfile(source.with_suffix('.dxf'))
    audit = doc.audit()
    assert not audit.errors and not audit.fixes, (audit.errors, audit.fixes)
    return doc

def elements(svg):
    return list(svg.find('s:g', NS))

def numbers(element):
    return [float(x) for x in re.findall(r'[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?', element.get('d'))]

def close(a, b):
    assert math.isclose(a, b, rel_tol=1e-9, abs_tol=1e-8), (a, b)

# 従来形式とオブジェクト形式は同じ円。直接描画とpathの同等性。
a = elements(run('drawing.circle(20,30,10);drawing.circle(20,30,{radius:10});'))
assert a[0].attrib == a[1].attrib
for opts in ['{radius:10}', '{radiusX:30,radiusY:10}', '{radiusX:10,radiusY:30}',
             '{radius:10,start:350,end:10}', '{radiusX:10,radiusY:30,start:0,end:270,clockwise:false}']:
    a = elements(run(f'drawing.circle(20,30,{opts});drawing.draw(path.circle(20,30,{opts}));'))
    assert a[0].tag == a[1].tag and a[0].attrib == a[1].attrib

# 部分円弧はAコマンド1つ、閉じない。外接矩形は部分だけ。
svg = run('drawing.circle(0,0,{radius:20,start:0,end:90});')
el = elements(svg)[0]; n = numbers(el)
assert el.tag.endswith('path') and 'Z' not in el.get('d')
for actual, expected in zip(n, [20,0,20,20,0,0,1,0,20]): close(actual, expected)
for actual, expected in zip(map(float, svg.get('viewBox').split()), [-1,-1,22,22]): close(actual, expected)
el = elements(run('drawing.circle(0,0,{radius:20,start:0,end:270,clockwise:false});'))[0]
n = numbers(el); assert n[5:7] == [0,0];close(n[-2],0);close(n[-1],-20)
el = elements(run('drawing.circle(0,0,{radius:20,start:350,end:10});'))[0]
assert numbers(el)[5:7] == [0,1]
# 長軸がYでも開始角0は右。360度は閉じた楕円のまま。
el = elements(run('drawing.circle(0,0,{radiusX:10,radiusY:30,start:0,end:90});'))[0]
n = numbers(el);close(n[0],10);close(n[1],0);close(n[-2],0);close(n[-1],30)
assert elements(run('drawing.circle(0,0,{radiusX:10,radiusY:30});'))[0].tag.endswith('ellipse')
# 任意の回転・非等方倍率・鏡面反転でも円弧端点を正確に保持する。
for rx, ry in [(10,10),(10,30),(30,10)]:
    for sx, sy in [(150,80),(-100,100),(100,-100),(-140,-60)]:
        svg = run(f'var p=path.circle(2,3,{{radiusX:{rx},radiusY:{ry},start:25,end:240}});'
                  f'drawing.draw(path.transformed(p,{{scale:[{sx},{sy}],rot:37,pos:[7,-4],anchor:[0,0]}}));')
        n = numbers(elements(svg)[0])
        for t, actual in [(25,n[:2]),(240,n[-2:])]:
            x=(2+rx*math.cos(math.radians(t)))*sx/100
            y=(3+ry*math.sin(math.radians(t)))*sy/100
            c=math.cos(math.radians(37));s=math.sin(math.radians(37))
            close(actual[0],c*x-s*y+7);close(actual[1],s*x+c*y-4)
        assert n[6] == (1 if sx*sy > 0 else 0)
        # 部分円弧の外接矩形が端点・途中の極値を含み、余計な全周領域にならない。
        points=[]
        for i in range(10001):
            t=math.radians(25+(240-25)*i/10000)
            x=(2+rx*math.cos(t))*sx/100; y=(3+ry*math.sin(t))*sy/100
            points.append((c*x-s*y+7,s*x+c*y-4))
        vx,vy,vw,vh=map(float,svg.get('viewBox').split())
        expected=[min(x for x,y in points)-1,min(y for x,y in points)-1,
                  max(x for x,y in points)+1,max(y for x,y in points)+1]
        for actual,want in zip([vx,vy,vx+vw,vy+vh],expected):
            assert abs(actual-want)<1e-5,(actual,want)
# コピー、移動、スタイル、既存の閉じた領域合成。
svg=run('var p=path.circle(0,0,{radius:10,end:90});drawing.draw(p);drawing.draw(p.moved(20,30));')
a,b=map(numbers,elements(svg));close(b[0]-a[0],20);close(b[1]-a[1],30)
run('drawing.draw(path.combine("xor",path.circle(0,0,{radiusX:20,radiusY:10}),path.rect(0,0,20,20)));')
run('drawing.draw(path.svgcombine("xor",path.circle(0,0,{radiusX:20,radiusY:10}),path.circle(15,0,10)));')
# 入力エラーと開いた円弧に対する塗り・合成の拒否。
for opts in ['{}','{radius:0}','{radiusX:10}','{radiusY:10}','{radiusX:-1,radiusY:10}',
             '{radius:10,start:0,end:0}','{radius:10,end:361}','{radius:10,start:NaN}',
             '{radius:10,clockwise:1}','{get radius(){throw Error("getter")}}']:
    run(f'drawing.circle(0,0,{opts});',success=False)
run('drawing.circle(0,0,{radius:10,end:90},{fill:"#ff0000"});',success=False)
run('drawing.draw(path.circle(0,0,{radius:10,end:90}),{fill:"#ff0000"});',success=False)
for op in ['combine','svgcombine']:
    run(f'path.{op}("or",path.circle(0,0,{{radius:10,end:90}}),path.rect(0,0,10,10));',success=False)
run('drawing.combine("or",()=>{drawing.circle(0,0,{radius:10,end:90});drawing.rect(0,0,10,10);});',success=False)
# DXFはARC/部分ELLIPSE、Fusionは円弧をARC、楕円弧を開いた折れ線で保持。
for clockwise in ['true','false']:
    doc=run(f'drawing.circle(0,0,{{radius:20,start:0,end:90,clockwise:{clockwise}}});','djs')
    arc=list(doc.modelspace())[0];assert arc.dxftype()=='ARC'
    close(arc.dxf.start_angle,270 if clockwise=='true' else 0)
    close(arc.dxf.end_angle,0 if clockwise=='true' else 270)
for rx,ry in [(30,10),(10,30)]:
    code=f'drawing.circle(0,0,{{radiusX:{rx},radiusY:{ry},start:0,end:90}});'
    doc=run(code,'djs');ellipse=list(doc.modelspace())[0];assert ellipse.dxftype()=='ELLIPSE'
    close(ellipse.end_point.x,rx);close(ellipse.end_point.y,0)
    close(ellipse.start_point.x,0);close(ellipse.start_point.y,-ry)
    doc=run(code,'djs','--fusion');line=list(doc.modelspace())[0]
    assert line.dxftype()=='LWPOLYLINE' and not line.closed
    pts=list(line.get_points());close(pts[0][0],rx);close(pts[-1][1],-ry)
doc=run('drawing.setUnit("px");drawing.setDpi(96);drawing.circle(0,0,{radius:96,end:90});','djs')
close(list(doc.modelspace())[0].dxf.radius,25.4)
print('Circle/ellipse/arcs: geometry, direction, transforms, validation, SVG/DXF/Fusion OK')
