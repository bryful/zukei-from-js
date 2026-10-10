"""端点、正負offset、穴・DXF、破綻時の停止を検証する。"""
from pathlib import Path
import math,re,subprocess,sys,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'tests/.deps'))
import ezdxf
EXE=ROOT/'x64/Release/zfj.exe'
OUT=ROOT/'tests/path-output/offsets';OUT.mkdir(parents=True,exist_ok=True)
NS={'s':'http://www.w3.org/2000/svg'}
def run(code,ext='sjs',*args,success=True):
 p=OUT/f'case.{ext}';p.write_text(code,encoding='utf-8')
 result=subprocess.run([str(EXE),str(p),'--force',*args],capture_output=True,text=True)
 assert (result.returncode==0)==success,(code,result.stderr)
 if not success:return result.stderr
 if ext=='sjs':return ET.parse(p.with_suffix('.svg')).getroot()
 doc=ezdxf.readfile(p.with_suffix('.dxf'));audit=doc.audit()
 assert not audit.errors and not audit.fixes,(audit.errors,audit.fixes)
 return doc
def shapes(svg):return list(svg.find('s:g',NS))
def loops(e):return [[tuple(map(float,p)) for p in re.findall(r'[ML]([-+\d.eE]+) ([-+\d.eE]+)',part)] for part in e.get('d').split('Z') if part.strip()]
def area(p):return sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(p,p[1:]+p[:1]))/2
def bounds(p):return [min(x for x,y in p),min(y for x,y in p),max(x for x,y in p),max(y for x,y in p)]
def near(a,b,tol=1e-7):assert math.isclose(a,b,abs_tol=tol,rel_tol=tol),(a,b)
for cap,want,expected in [('butt',[0,-2,10,2],40),('square',[-2,-2,12,2],56),('round',[-2,-2,12,2],40+math.pi*4)]:
 e=shapes(run(f'drawing.draw(path.offset(path.line(0,0,10,0),2,{{cap:"{cap}"}}),{{fill:"#66aaff"}});'))[0];p=loops(e)[0]
 for a,b in zip(bounds(p),want):near(a,b,.011 if cap=='round' else 1e-7)
 near(abs(area(p)),expected,.01 if cap=='round' else 1e-7)
svg=run('var p=path.line(0,0,10,0);drawing.draw(p);var q=path.offset(p,2);if(q===p)throw Error("identity");drawing.draw(q);q.move(20,30);drawing.draw(q);drawing.draw(p);')
e=shapes(svg);assert e[0].tag.endswith('line') and e[3].attrib==e[0].attrib
assert bounds(loops(e[1])[0])==[0,-2,10,2] and bounds(loops(e[2])[0])==[20,28,30,32]
for delta,expected in [(2,196),(-2,36),(0,100)]:
 e=shapes(run(f'drawing.draw(path.offset(path.rect(0,0,10,10),{delta}));'))[0];near(sum(area(p) for p in loops(e)),expected)
for points in ['[[0,0],[0,10],[10,10],[10,0]]','[[0,0],[10,0],[10,10],[0,10],[0,0]]']:
 e=shapes(run(f'drawing.draw(path.offset(path.polygon({points}),2));'))[0];near(sum(area(p) for p in loops(e)),196)
for join in ['miter','round','bevel']:run(f'drawing.draw(path.offset(path.polyline([[0,0],[10,0],[10,10]]),1,{{join:"{join}"}}));')
e=shapes(run('drawing.draw(path.offset(path.circle(10,10,5),2));'))[0];assert e.tag.endswith('circle');near(float(e.get('r')),7)
for opts in ['{radiusX:20,radiusY:10}','{radius:20,start:0,end:90}','{radiusX:20,radiusY:10,start:25,end:230}']:run(f'drawing.draw(path.offset(path.circle(0,0,{opts}),1,{{cap:"round"}}));')
ring='path.combine("xor",path.rect(0,0,100,100),path.rect(30,30,40,40))'
source=f'var p=path.offset({ring},5);drawing.draw(p,{{fill:"#66aaff",strokeWidth:.5}});'
e=shapes(run(source))[0];assert len(loops(e))==2;near(sum(area(p) for p in loops(e)),12100-900)
for expr in ['path.combine("or",path.offset(path.rect(0,0,10,10),1),path.rect(5,0,10,10))','path.svgcombine("xor",path.offset(path.rect(0,0,10,10),1),path.circle(10,0,3))','path.offset(path.offset(path.rect(0,0,10,10),1),1)']:run(f'drawing.draw({expr},{{fill:"#66aaff"}});')
for prefix in ['', 'drawing.setUnit("pt");', 'drawing.setUnit("px");drawing.setDpi(96);']:
 doc=run(prefix+source,'djs');entities=list(doc.modelspace())
 assert [x.dxftype() for x in entities]==['HATCH','LWPOLYLINE','LWPOLYLINE']
 assert sorted(x.path_type_flags for x in entities[0].paths)==[2,3] and all(x.closed for x in entities[1:])
 doc=run(prefix+source,'djs','--fusion');assert len(doc.modelspace())==2 and all(x.dxftype()=='LWPOLYLINE' and x.closed for x in doc.modelspace())
mirrored=f'var p=path.offset({ring},5);path.transform(p,{{scale:[-100,100]}});drawing.draw(p,{{fill:"#66aaff"}});'
hatch=list(run(mirrored,'djs').modelspace())[0];assert sorted(x.path_type_flags for x in hatch.paths)==[2,3]
e=shapes(run('drawing.setUnit("px");drawing.setDpi(96);drawing.draw(path.offset(path.rect(0,0,10,10),2));','sjs','--unit','mm'))[0];assert bounds(loops(e)[0])==[-2,-2,12,12]
bad=['path.offset(path.line(0,0,10,0),0);','path.offset(path.line(0,0,10,0),-1);','path.offset(path.line(0,0,0,0),1);','path.offset(path.rect(0,0,10,10),-5);','path.offset(path.circle(0,0,5),-5);','path.offset(path.rect(0,0,10,10),NaN);','path.offset(path.rect(0,0,10,10),1,{cap:"bad"});','path.offset(path.rect(0,0,10,10),1,{join:"bad"});','path.offset(path.rect(0,0,10,10),1,null);','path.offset(path.polygon([[0,0],[10,10],[0,10],[10,0]]),1);','path.offset(path.polyline([[0,0],[10,0],[5,0]]),1);','path.offset(path.polyline([[0,0],[10,10],[0,10],[10,0]]),1);','path.offset(path.svgcombine("xor",path.rect(0,0,10,10),path.circle(10,0,5)),1);',f'path.offset({ring},21);','path.offset(path.combine("or",path.rect(0,0,10,10),path.rect(15,0,10,10)),3);','path.offset(path.polygon([[0,0],[10,0],[10,4],[20,4],[20,0],[30,0],[30,10],[20,10],[20,6],[10,6],[10,10],[0,10]]),-2);','path.offset(path.rect(0,0,10,10),.00001);','path.offset(path.circle(1e9,0,1),2);']
for code in bad:run(code,success=False)
svg=run('var p=path.rect(0,0,10,10);try{path.offset(p,-6);}catch(e){}drawing.draw(p);');assert len(shapes(svg))==1
p=OUT/'case.sjs';dest=p.with_suffix('.svg');dest.write_text('sentinel',encoding='utf-8');p.write_text('drawing.draw(path.rect(0,0,10,10));path.offset(path.rect(0,0,10,10),-6);',encoding='utf-8')
result=subprocess.run([str(EXE),str(p),'--force'],capture_output=True,text=True);assert result.returncode==1 and dest.read_text()=='sentinel'
print('Offset tests passed: caps, open/closed paths, holes, transforms, SVG/DXF and failures.')
