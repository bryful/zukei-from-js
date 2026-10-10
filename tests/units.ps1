$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'x64/Release/zfj.exe'
$tempDir = Join-Path $PSScriptRoot 'unit-output'
New-Item $tempDir -ItemType Directory -Force | Out-Null
function Near($actual, $expected) { if ([math]::Abs($actual - $expected) -gt 0.000001) { throw "Expected $expected, got $actual" } }
try {
    $source = 'drawing.line(0,0,72,36); drawing.rect(0,0,72,36); drawing.circle(36,18,9); drawing.polygon([[0,0],[72,0],[0,36]]);'
    foreach ($ext in @('sjs','djs')) { Set-Content (Join-Path $tempDir "test.$ext") $source -Encoding utf8 }
    foreach ($unit in @('mm','pt','px')) {
        & $exe (Join-Path $tempDir 'test.sjs') --unit $unit -o (Join-Path $tempDir "$unit.svg") --force
        if ($LASTEXITCODE -ne 0) { throw 'SVG generation failed' }
        & $exe (Join-Path $tempDir 'test.djs') --unit $unit -o (Join-Path $tempDir "$unit.dxf") --force
        if ($LASTEXITCODE -ne 0) { throw 'DXF generation failed' }
        [xml]$svg = Get-Content (Join-Path $tempDir "$unit.svg") -Raw
        $factor = if ($unit -eq 'mm') { 1.0 } else { 25.4/72 }
        $suffix = if ($unit -eq 'mm') { 'mm' } else { 'pt' }
        if (-not $svg.svg.width.EndsWith($suffix)) { throw 'SVG unit suffix mismatch' }
        Near ([double]($svg.svg.width.Replace($suffix,'')) * $factor) (72*$factor+2)
        Near ([double]$svg.svg.g.'stroke-width' * $factor) 0.25
        Near ([double]$svg.svg.g.line.x2) 72
        $tags=Get-Content (Join-Path $tempDir "$unit.dxf")
        $entities=@(); $entity=$null
        for($i=0;$i -lt $tags.Count;$i+=2) {
            $code=$tags[$i]; $value=$tags[$i+1]
            if($code -eq '0') { if($null -ne $entity){$entities+=,$entity};$entity=@{type=$value} }
            elseif($null -ne $entity) { $entity[$code]=$value }
        }
        $line=$entities | Where-Object type -eq 'LINE'
        Near ([double]$line['11']) (72*$factor)
        Near ([double]$line['21']) (36*$factor)
        $circle=$entities | Where-Object type -eq 'CIRCLE'
        Near ([double]$circle['40']) (9*$factor)
        $polylines=@($entities | Where-Object type -eq 'LWPOLYLINE')
        if($polylines.Count -ne 2){throw 'Closed polygon count mismatch'}
        if($polylines[0]['90'] -ne '4' -or $polylines[1]['90'] -ne '3'){throw 'Polygon vertex count mismatch'}
        Near ([double]$polylines[0]['20']) (36*$factor)
    }
    & $exe (Join-Path $tempDir 'test.sjs') --check --unit inches
    if($LASTEXITCODE -ne 1){throw 'Invalid unit accepted'}
    & $exe (Join-Path $tempDir 'test.sjs') --check --unit
    if($LASTEXITCODE -ne 1){throw 'Missing unit accepted'}
    & $exe (Join-Path $tempDir 'test.sjs') -o (Join-Path $tempDir 'default.svg') --force
    if($LASTEXITCODE -ne 0){throw 'Default generation failed'}
    if((Get-Content (Join-Path $tempDir 'default.svg') -Raw) -ne (Get-Content (Join-Path $tempDir 'mm.svg') -Raw)){throw 'Default mm changed'}
    foreach($ext in @('sjs','djs')) {
        $outExt=if($ext -eq 'sjs'){'svg'}else{'dxf'}
        foreach($jsUnit in @('mm','pt','px')) {
            $inputPath=Join-Path $tempDir "script.$ext"
            Set-Content $inputPath ("drawing.setUnit(`"$jsUnit`");"+$source) -Encoding utf8
            foreach($cliUnit in @('', 'mm','pt','px')) {
                $outputPath=Join-Path $tempDir "script.$outExt"
                $arguments=@($inputPath,'-o',$outputPath,'--force')
                if($cliUnit){$arguments+=@('--unit',$cliUnit)}
                & $exe @arguments
                if($LASTEXITCODE -ne 0){throw 'JS unit generation failed'}
                $effective=if($cliUnit){$cliUnit}else{$jsUnit}
                if((Get-Content $outputPath -Raw) -ne (Get-Content (Join-Path $tempDir "$effective.$outExt") -Raw)) {
                    throw "Unit precedence failed: JS=$jsUnit CLI=$cliUnit format=$ext"
                }
            }
        }
    }
    foreach($bad in @('drawing.setUnit("inch");','drawing.setUnit(72);','drawing.setUnit();')) {
        Set-Content (Join-Path $tempDir 'invalid.sjs') $bad -Encoding utf8
        & $exe (Join-Path $tempDir 'invalid.sjs') --check --unit mm
        if($LASTEXITCODE -ne 1){throw 'Invalid JS unit accepted'}
    }
    foreach($dpi in @(72,96,300,144.5)) {
        foreach($unit in @('px','pt','mm')) {
            $factor=if($unit -eq 'px'){25.4/$dpi}elseif($unit -eq 'pt'){25.4/72}else{1}
            $script="drawing.setUnit(`"$unit`");drawing.setDpi($dpi);drawing.line(0,0,96,48);"
            Set-Content (Join-Path $tempDir 'dpi.sjs') $script -Encoding utf8
            Set-Content (Join-Path $tempDir 'dpi.djs') $script -Encoding utf8
            & $exe (Join-Path $tempDir 'dpi.sjs') --force
            if($LASTEXITCODE -ne 0){throw 'DPI SVG failed'}
            [xml]$doc=Get-Content (Join-Path $tempDir 'dpi.svg') -Raw
            $physicalFactor=if($unit -eq 'mm'){1}else{25.4/72}
            Near ([double]($doc.svg.width -replace '(mm|pt)$','')*$physicalFactor) (96*$factor+2)
            Near ([double]$doc.svg.g.'stroke-width'*$factor) 0.25
            & $exe (Join-Path $tempDir 'dpi.djs') --force
            if($LASTEXITCODE -ne 0){throw 'DPI DXF failed'}
            $tags=Get-Content (Join-Path $tempDir 'dpi.dxf')
            for($i=0;$i -lt $tags.Count;$i+=2){if($tags[$i] -eq '11'){Near ([double]$tags[$i+1]) (96*$factor)}}
        }
    }
    foreach($bad in @('0','-1','NaN','Infinity','"96"','1000001')) {
        Set-Content (Join-Path $tempDir 'invalid.sjs') "drawing.setDpi($bad);" -Encoding utf8
        & $exe (Join-Path $tempDir 'invalid.sjs') --check
        if($LASTEXITCODE -ne 1){throw 'Invalid DPI accepted'}
    }
    'Unit tests passed'
} finally { Get-ChildItem -LiteralPath $tempDir -File | Remove-Item }



