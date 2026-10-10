"""SVGの領域面積・穴・ネスト・単位とDXFで合成しないことを検証する。"""
from pathlib import Path
import math
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent.parent
EXE=ROOT/'x64/Release/zfj.exe'
OUT=ROOT/'tests/path-output'
OUT.mkdir(exist_ok=True)
NS={'s':'http://www.w3.org/2000/svg'}


def run(source,ext='sjs',*args,success=True):
    """ソースを実行し、成功時は生成ファイルの内容を返す。"""
    path=OUT/f'case.{ext}'
    path.write_text(source,encoding='utf-8')
    result=subprocess.run([str(EXE),str(path),'--force',*args],capture_output=True,text=True)
    assert (result.returncode==0)==success,result.stderr
    return path.with_suffix('.svg' if ext=='sjs' else '.dxf').read_text() if success and '--check' not in args else ''


def contours(path):
    """SVGのM/L/Zから輪郭を取り出す（出力は全て直線）。"""
    result=[]
    for segment in path.get('d').split('M')[1:]:
        numbers=list(map(float,re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?',segment)))
        result.append(list(zip(numbers[::2],numbers[1::2])))
    return result


def area(path):
    """穴の逆向き輪郭を含む符号付き面積の合計を求める。"""
    total=0
    for points in contours(path):
        total+=sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(points,points[1:]+points[:1]))/2
    return abs(total)


base='drawing.rect(0,0,10,10);drawing.rect(5,0,10,10);'
style='{stroke:"#ff0000",fill:"#00aa66",strokeWidth:0.5}'
for operation,expected in [('or',150),('and',50),('xor',100)]:
    svg=ET.fromstring(run(f'drawing.combine("{operation}",()=>{{{base}}},{style});'))
    paths=svg.findall('s:g/s:path',NS)
    assert len(paths)==1
    assert math.isclose(area(paths[0]),expected,abs_tol=1e-8)
    assert paths[0].get('stroke')=='#ff0000'
    assert paths[0].get('fill')=='#00aa66'
    assert paths[0].get('stroke-width')=='0.5'
    # DXFはグループのスタイルを適用せず、元の二つの四角形と同一。
    assert run(f'drawing.combine("{operation}",()=>{{{base}}},{style});','djs')==run(base,'djs')
    assert run(f'drawing.combine("{operation}",()=>{{{base}}});','djs','--fusion')==run(base,'djs','--fusion')

# XORは内側を穴として残し、ANDの非重複は空のSVGになる。
svg=ET.fromstring(run('drawing.combine("xor",()=>{drawing.rect(0,0,10,10);drawing.rect(2,2,5,5);},{fill:"#000000"});'))
path=svg.find('s:g/s:path',NS)
assert len(contours(path))==2 and area(path)==75
svg=ET.fromstring(run('drawing.combine("and",()=>{drawing.rect(0,0,1,1);drawing.rect(10,10,1,1);});'))
assert not svg.findall('s:g/s:path',NS)
# 三つのANDは最初と残り全部の共通領域。
svg=ET.fromstring(run('drawing.combine("and",()=>{drawing.rect(0,0,10,10);drawing.rect(5,0,10,10);drawing.rect(7,0,10,10);});'))
assert area(svg.find('s:g/s:path',NS))==30
# ネストしたORの結果をANDの一方に使用する。
nested='drawing.combine("and",()=>{drawing.combine("or",()=>{'+base+'});drawing.rect(7,0,2,10);},{fill:"#000000"});'
assert area(ET.fromstring(run(nested)).find('s:g/s:path',NS))==20
assert run(nested,'djs').count('\nLWPOLYLINE\n')==3
# 円はSVGの合成時だけ近似され、DXFには円を二つ保持する。
circles='drawing.circle(0,0,10);drawing.circle(0,0,10);'
path=ET.fromstring(run('drawing.combine("or",()=>{'+circles+'});')).find('s:g/s:path',NS)
assert 0 < math.pi*100-area(path) < 0.7
assert run('drawing.combine("or",()=>{'+circles+'});','djs').count('\nCIRCLE\n')==2
# 合成結果の入力単位、固定キャンバスとCLI優先順位を維持する。
setup='drawing.setUnit("px");drawing.setDpi(96);drawing.setCanvas(100,80);'
svg=ET.fromstring(run(setup+'drawing.combine("or",()=>{'+base+'});'))
assert svg.get('width')=='100px' and svg.get('viewBox')=='0 0 100 80'
assert math.isclose(area(svg.find('s:g/s:path',NS)),150,abs_tol=0.02)
svg=ET.fromstring(run(setup+'drawing.combine("or",()=>{'+base+'});','sjs','--unit','mm'))
assert area(svg.find('s:g/s:path',NS))==150
for bad in [
    'drawing.combine("bad",()=>{});',
    'drawing.combine("or",()=>{drawing.rect(0,0,1,1);});',
    'drawing.combine("or",()=>{drawing.line(0,0,1,1);drawing.rect(0,0,1,1);});',
    'drawing.combine("or",()=>{throw Error("test");});',
    'drawing.combine("or",async()=>{drawing.rect(0,0,1,1);drawing.rect(0,0,1,1);});'
]:
    run(bad,'sjs','--check',success=False)
# エラーをJSで捕捉した場合、途中の図形は残さない。
svg=ET.fromstring(run('try{drawing.combine("or",()=>{drawing.rect(0,0,1,1);throw Error("test");});}catch(e){}drawing.circle(0,0,1);'))
assert len(list(svg.find('s:g',NS)))==1
print('Path boolean tests passed.')

# 値ベースAPI: 作成だけでは描画しない。drawした時だけ図面へ追加する。
assert len(list(ET.fromstring(run('var a=path.rect(0,0,100,100);')).find('s:g',NS)))==0
for operation in ['and','or','xor']:
    value_source=f'var a=path.rect(0,0,10,10);var b=path.rect(5,0,10,10);var v=path.combine("{operation}",a,b);drawing.draw(v,{style});'
    legacy_source=f'drawing.combine("{operation}",()=>{{{base}}},{style});'
    assert run(value_source)==run(legacy_source)
    assert run(value_source,'djs')==run(base,'djs')
# primitiveパスのスタイル、複数回drawによる再利用とスタイル上書きの独立性。
reuse='var a=path.circle(10,10,5,{stroke:"#112233",fill:"#abcdef"});drawing.draw(a,{fill:"#ff0000"});drawing.draw(a);'
svg=ET.fromstring(run(reuse))
shapes=list(svg.find('s:g',NS))
assert len(shapes)==2 and shapes[0].get('fill')=='#ff0000' and shapes[1].get('fill')=='#abcdef'
# line / polygonも値として生成できる。
new='drawing.draw(path.line(0,0,10,10));drawing.draw(path.polygon([[0,0],[10,0],[0,10]]));'
old='drawing.line(0,0,10,10);drawing.polygon([[0,0],[10,0],[0,10]]);'
assert run(new)==run(old)
# 一つのオペランドを複数回使い、合成結果をさらに合成できる。
new='var a=path.rect(0,0,10,10);var b=path.rect(5,0,10,10);var v=path.combine("or",a,b);var c=path.rect(7,0,2,10);drawing.draw(path.combine("and",v,c));'
assert area(ET.fromstring(run(new)).find('s:g/s:path',NS))==20
assert run(new,'djs').count('\nLWPOLYLINE\n')==3
assert len(list(ET.fromstring(run('var a=path.rect(0,0,1,1);drawing.draw(path.combine("xor",a,a));')).find('s:g',NS)))==0
# 3個以上のオペランドも許容する。
assert area(ET.fromstring(run('var a=path.rect(0,0,10,10);drawing.draw(path.combine("or",a,a,a));')).find('s:g/s:path',NS))==100
for bad in [
    'drawing.draw({});', 'drawing.draw(null);', 'drawing.draw(123);',
    'path.combine("xor",path.rect(0,0,1,1),{});',
    'path.combine("or",path.line(0,0,1,1),path.rect(0,0,1,1));',
    'path.combine("bad",path.rect(0,0,1,1),path.rect(0,0,1,1));',
    'path.combine("or",path.rect(0,0,1,1));',
    'drawing.draw(path.rect(0,0,1,1),{strokeWidth:0});'
]:
    run(bad,'sjs','--check',success=False)
print('Path value API tests passed.')

# SVG表示合成は元図形とマスクを保持し、Clipper2の折れ線パスに置き換えない。
for operation in ['or', 'and', 'xor']:
    source = f'var a=path.rect(0,0,10,10);var b=path.circle(10,5,5);drawing.draw(path.svgcombine("{operation}",a,b),{style});'
    svg = ET.fromstring(run(source))
    if operation == 'xor':
        compound = svg.find('s:g/s:path', NS)
        assert compound is not None and compound.get('fill-rule') == 'evenodd'
        assert compound.get('stroke-width') == '0.5' and compound.get('stroke') == '#ff0000'
        assert compound.get('d').count('M') == 2 and compound.get('d').count('A') == 2
        assert not svg.findall('.//s:mask', NS) and not svg.findall('.//s:filter', NS)
        assert run(source, 'djs') == run('drawing.rect(0,0,10,10);drawing.circle(10,5,5);', 'djs')
        continue
    if operation != 'or':
        assert svg.find('.//s:mask', NS) is not None
    assert len(svg.findall('.//s:defs//s:circle', NS)) == 1
    artwork = svg.find('.//s:g[@data-zfj-artwork="originals"]', NS)
    assert artwork is not None
    assert artwork.find('.//s:g[@data-zfj-source="original"]/s:rect', NS) is not None
    if operation in ['or', 'xor']:
        assert artwork.find('.//s:g[@data-zfj-source="original"]/s:circle', NS) is not None
    # 塗り用の全面rectではなく、元の座標・寸法の図形が描画対象になっている。
    assert artwork.find('s:rect', NS) is None
    original = artwork.find('.//s:g[@data-zfj-source="original"]/s:rect', NS)
    assert original.get('x') == '0' and original.get('width') == '10'
    assert not svg.findall('.//s:path', NS)
    assert not svg.findall('.//s:feColorMatrix', NS)
    if operation == 'xor':
        assert len([node for node in svg.findall('.//s:mask/s:use', NS)
                    if node.get('fill') == 'black']) == 2
    assert svg.find('s:g/s:g', NS).get('data-zfj-combine') == 'svg'
    assert run(source, 'djs') == run('drawing.rect(0,0,10,10);drawing.circle(10,5,5);', 'djs')
    # 定義IDは重複せず、参照が全て存在する。
    ids = [node.get('id') for node in svg.iter() if node.get('id')]
    assert len(ids) == len(set(ids))
    for node in svg.iter():
        for value in node.attrib.values():
            if value.startswith('#zfj'):
                assert value[1:] in ids
            for reference in re.findall(r'url\(#([^)]*)\)', value):
                assert reference in ids

# SVG合成の中に計算済みパスを置くことはできる。逆は意味が変わるため明示エラー。
source = 'var a=path.circle(5,5,5);var b=path.rect(0,0,10,10);var c=path.combine("and",a,b);drawing.draw(path.svgcombine("xor",c,a));'
svg = ET.fromstring(run(source))
assert svg.findall('.//s:path', NS) and svg.findall('.//s:circle', NS)
run('var a=path.rect(0,0,10,10);var v=path.svgcombine("or",a,a);drawing.draw(path.combine("and",v,a));', 'sjs', '--check', success=False)

# ブラウザーで同じ画像になることを検証するための対比較。上段がSVG表示合成、下段がClipper2。
rows = []
for api in ['svgcombine', 'combine']:
    cells = []
    for nested in [False, True]:
        for operation in ['or', 'and', 'xor']:
            setup = 'drawing.setUnit("px");drawing.setDpi(96);drawing.setCanvas(120,100);var a=path.rect(10,10,60,60);var b=path.rect(40,30,60,60);'
            if nested:
                setup += f'a=path.{api}("xor",a,b);b=path.rect(20,0,40,100);'
            svg = run(setup + f'drawing.draw(path.{api}("{operation}",a,b),{{stroke:null,fill:"#000000"}});')
            assert api != 'svgcombine' or not ET.fromstring(svg).findall('.//s:filter', NS)
            # 別SVG文書のIDをインラインHTMLで同居させるため、この比較用に接頭辞を付ける。
            cells.append(svg[svg.index('<svg '):].replace('zfj', f'{api}_{nested}_{operation}_zfj'))
    rows.append('<div style="display:flex">' + ''.join(cells) + '</div>')
(OUT/'comparison.html').write_text('<!doctype html><body style="margin:0;background:white">'+''.join(rows), encoding='utf-8')
print('SVG rendering composite tests passed.')

# 円・長方形XORは元輪郭を持つ複合パス。線は1個のstroke-widthで統一する。
source = 'var a=path.rect(10,10,70,70);var b=path.circle(75,55,30);drawing.draw(path.svgcombine("xor",a,b),{stroke:null,fill:"#66aaff"});'
svg = ET.fromstring(run(source))
compound = svg.find('s:g/s:path', NS)
assert compound is not None and compound.get('fill-rule') == 'evenodd'
assert compound.get('d').count('M') == 2 and compound.get('d').count('A') == 2
assert not svg.findall('.//s:mask', NS) and not svg.findall('.//s:filter', NS)
for width in [0.5, 2, 10]:
    svg = ET.fromstring(run(source.replace('stroke:null', f'stroke:"#003366",strokeWidth:{width}')))
    compound = svg.find('s:g/s:path', NS)
    assert compound.get('stroke-width') == str(width)
    assert compound.get('stroke') == '#003366'
    assert len(svg.findall('.//s:path', NS)) == 1
    assert not svg.findall('.//s:filter', NS)
print('Editable compound XOR stroke tests passed.')
