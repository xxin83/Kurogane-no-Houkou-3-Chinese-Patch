$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw

if ($source -notmatch 'kMinHistoricalParagraphSourceBytes\s*=\s*30') {
    throw 'Historical paragraph caching must define a source-byte threshold for zero-width measurements.'
}

if ($source -notmatch 'requestedWidth\s*<=\s*0\s*&&\s*sourceCount\s*>=\s*kMinHistoricalParagraphSourceBytes') {
    throw 'CachePatchParagraph must admit sufficiently long paragraphs measured with a zero-width rectangle.'
}

if ($source -notmatch 'requestedWidth\s*<=\s*0\s*&&\s*actualCount\s*>=\s*kMinHistoricalParagraphSourceBytes') {
    throw 'DrawTextA historical measurement must use the original byte count for zero-width paragraph caching.'
}

Write-Output 'zero-width paragraph cache contract: PASS'
