[CmdletBinding()]
param(
    [string]$RepositoryRoot,

    [string]$CommitRange
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-TemplateSecretValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $candidate = $Value.Trim()
    if ([string]::IsNullOrWhiteSpace($candidate) -or $candidate -match '^\*+$') {
        return $true
    }

    # Variables, placeholders, and regex examples are documentation/code, not concrete credentials.
    if ($candidate -match '[$%{}<>\[\]]') {
        return $true
    }

    return $candidate -match '(?i)^(?:EOS_CLIENT_SECRET|CLIENT_SECRET|YOUR[_-].*|REPLACE[_-]?ME|CHANGE[_-]?ME|SECRET[_-]?HERE|REDACTED|PLACEHOLDER|EXAMPLE|SAMPLE|DUMMY|NOT[_-]?A[_-]?SECRET)$'
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent $PSScriptRoot
}
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath (Join-Path $RepositoryRoot ".git"))) {
    throw "Git 저장소 루트를 찾을 수 없습니다: $RepositoryRoot"
}

$git = Get-Command git -ErrorAction Stop
$gitSafeRoot = $RepositoryRoot.Replace('\', '/')
$gitArguments = @('-c', "safe.directory=$gitSafeRoot", '-C', $RepositoryRoot)
$violations = [System.Collections.Generic.List[string]]::new()
$detectedLocations = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)

$artifactPattern = [regex]::new(
    '(?ims)^[ \t]*[+]?Artifacts[ \t]*=[ \t]*\((?<body>.*?)\)',
    [System.Text.RegularExpressions.RegexOptions]::Compiled)
$secretPattern = [regex]::new(
    '(?is)(?:^|,)[ \t\r\n]*ClientSecret[ \t]*=[ \t]*(?:"(?<quoted>[^"]*)"|(?<bare>[^,\)\r\n]*))',
    [System.Text.RegularExpressions.RegexOptions]::Compiled)
$assignmentPattern = [regex]::new(
    '(?im)["'']?(?:EOS[_-]?CLIENT[_-]?SECRET|Client[_-]?Secret)["'']?[ \t]*(?:=|:)[ \t]*(?:"(?<double>[^"\r\n]*)"|''(?<single>[^''\r\n]*)''|(?<bare>[^\s,\)\]\}\r\n#;]+))',
    [System.Text.RegularExpressions.RegexOptions]::Compiled)

function Invoke-LBHygieneGit {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$CommandArguments,

        [int[]]$AllowedExitCodes = @(0)
    )

    $output = @(& $git.Source @gitArguments @CommandArguments)
    $exitCode = $LASTEXITCODE
    if ($AllowedExitCodes -notcontains $exitCode) {
        throw "EOS 저장소 보안 검사용 git 명령이 실패했습니다. 종료 코드: $exitCode"
    }

    return $output
}

function Add-LBGeneratedConfigViolation {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathLabel
    )

    if ($detectedLocations.Add($PathLabel)) {
        $violations.Add(('{0}: 로컬 EOS 자격 파일이 Git에 추적되고 있습니다.' -f $PathLabel))
    }
}

function Test-LBSecretContent {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Content,

        [Parameter(Mandatory = $true)]
        [string]$PathLabel
    )

    foreach ($artifactMatch in $artifactPattern.Matches($content)) {
        $bodyGroup = $artifactMatch.Groups['body']
        foreach ($secretMatch in $secretPattern.Matches($bodyGroup.Value)) {
            $value = if ($secretMatch.Groups['quoted'].Success) {
                $secretMatch.Groups['quoted'].Value
            }
            else {
                $secretMatch.Groups['bare'].Value
            }

            if (Test-TemplateSecretValue -Value $value) {
                continue
            }

            $absoluteMatchIndex = $bodyGroup.Index + $secretMatch.Index
            $lineNumber = 1 + [regex]::Matches(
                $content.Substring(0, $absoluteMatchIndex),
                "\r\n|\n|\r").Count
            $location = '{0}:{1}' -f $PathLabel, $lineNumber
            if ($detectedLocations.Add($location)) {
                $violations.Add(('{0}: 구체적인 EOS Artifact ClientSecret이 감지되었습니다(값은 출력하지 않음).' -f $location))
            }
        }
    }

    foreach ($assignmentMatch in $assignmentPattern.Matches($content)) {
        $value = if ($assignmentMatch.Groups['double'].Success) {
            $assignmentMatch.Groups['double'].Value
        }
        elseif ($assignmentMatch.Groups['single'].Success) {
            $assignmentMatch.Groups['single'].Value
        }
        else {
            $assignmentMatch.Groups['bare'].Value
        }

        if (Test-TemplateSecretValue -Value $value) {
            continue
        }

        $lineNumber = 1 + [regex]::Matches(
            $content.Substring(0, $assignmentMatch.Index),
            "\r\n|\n|\r").Count
        $location = '{0}:{1}' -f $PathLabel, $lineNumber
        if ($detectedLocations.Add($location)) {
            $violations.Add(('{0}: 구체적인 EOS Client Secret 할당이 감지되었습니다(값은 출력하지 않음).' -f $location))
        }
    }
}

function Test-LBWorktreeSnapshot {
    $candidateFiles = Invoke-LBHygieneGit `
        -CommandArguments @(
            'grep', '--untracked', '-I', '-i', '-l', '-F',
            '-e', 'ClientSecret',
            '-e', 'CLIENT_SECRET',
            '--') `
        -AllowedExitCodes @(0, 1)

    foreach ($candidateFile in $candidateFiles) {
        $absolutePath = Join-Path $RepositoryRoot $candidateFile
        if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
            continue
        }

        $content = Get-Content -LiteralPath $absolutePath -Raw -Encoding UTF8
        Test-LBSecretContent `
            -Content $content `
            -PathLabel ('WORKTREE:{0}' -f $candidateFile.Replace('\', '/'))
    }
}

function Test-LBIndexSnapshot {
    $trackedFiles = Invoke-LBHygieneGit -CommandArguments @('ls-files')
    foreach ($trackedFile in $trackedFiles) {
        $normalizedPath = $trackedFile.Replace('\', '/')
        if ($normalizedPath -match '(?i)^Config/GeneratedEngine\.ini(?:$|\.)') {
            Add-LBGeneratedConfigViolation -PathLabel ('INDEX:{0}' -f $normalizedPath)
        }
    }

    $candidateFiles = Invoke-LBHygieneGit `
        -CommandArguments @(
            'grep', '--cached', '-I', '-i', '-l', '-F',
            '-e', 'ClientSecret',
            '-e', 'CLIENT_SECRET',
            '--') `
        -AllowedExitCodes @(0, 1)

    foreach ($candidateFile in $candidateFiles) {
        $blobSpec = ':{0}' -f $candidateFile
        $content = [string]::Join(
            [Environment]::NewLine,
            (Invoke-LBHygieneGit -CommandArguments @('show', $blobSpec)))
        Test-LBSecretContent `
            -Content $content `
            -PathLabel ('INDEX:{0}' -f $candidateFile.Replace('\', '/'))
    }
}

function Test-LBCommitSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Revision
    )

    $shortRevision = $Revision.Substring(0, [Math]::Min(12, $Revision.Length))
    $trackedFiles = Invoke-LBHygieneGit `
        -CommandArguments @('ls-tree', '-r', '--name-only', $Revision)
    foreach ($trackedFile in $trackedFiles) {
        $normalizedPath = $trackedFile.Replace('\', '/')
        if ($normalizedPath -match '(?i)^Config/GeneratedEngine\.ini(?:$|\.)') {
            Add-LBGeneratedConfigViolation `
                -PathLabel ('COMMIT {0}:{1}' -f $shortRevision, $normalizedPath)
        }
    }

    $candidateRefs = Invoke-LBHygieneGit `
        -CommandArguments @(
            'grep', '-I', '-i', '-l', '-F',
            '-e', 'ClientSecret',
            '-e', 'CLIENT_SECRET',
            $Revision,
            '--') `
        -AllowedExitCodes @(0, 1)
    $revisionPrefix = '{0}:' -f $Revision

    foreach ($candidateRef in $candidateRefs) {
        if (-not $candidateRef.StartsWith(
                $revisionPrefix,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        $candidateFile = $candidateRef.Substring($revisionPrefix.Length)
        $blobSpec = '{0}:{1}' -f $Revision, $candidateFile
        $content = [string]::Join(
            [Environment]::NewLine,
            (Invoke-LBHygieneGit -CommandArguments @('show', $blobSpec)))
        Test-LBSecretContent `
            -Content $content `
            -PathLabel ('COMMIT {0}:{1}' -f $shortRevision, $candidateFile.Replace('\', '/'))
    }
}

Test-LBWorktreeSnapshot
Test-LBIndexSnapshot

if (-not [string]::IsNullOrWhiteSpace($CommitRange)) {
    if ($CommitRange -cnotmatch '^[0-9a-fA-F]{7,40}(?:\.\.[0-9a-fA-F]{7,40})?$') {
        throw "CommitRange 형식이 올바르지 않습니다."
    }

    $commits = Invoke-LBHygieneGit `
        -CommandArguments @('rev-list', '--reverse', $CommitRange)
    foreach ($commit in $commits) {
        Test-LBCommitSnapshot -Revision $commit
    }
}

if ($violations.Count -gt 0) {
    Write-Host "EOS 저장소 보안 검사에 실패했습니다." -ForegroundColor Red
    foreach ($violation in $violations) {
        Write-Host " - $violation" -ForegroundColor Red
    }
    Write-Host "노출된 자격값은 즉시 EOS Developer Portal에서 폐기·교체하고 Git 기록 정리는 저장소 관리자와 진행하세요." -ForegroundColor Yellow
    exit 1
}

Write-Host "EOS 저장소 보안 검사 통과: Git 추적/index/비무시 worktree/요청 이력에 GeneratedEngine.ini 및 구체적인 EOS Client Secret 없음." -ForegroundColor Green
exit 0
