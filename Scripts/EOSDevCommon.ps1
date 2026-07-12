Set-StrictMode -Version Latest

$script:LBCommonScriptRoot = $PSScriptRoot

function Get-LBProjectContext {
    $projectRoot = Split-Path -Parent $script:LBCommonScriptRoot
    return [pscustomobject]@{
        ProjectRoot = [System.IO.Path]::GetFullPath($projectRoot)
        ProjectPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "LeftBehind.uproject"))
        GeneratedConfigPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "Config\GeneratedEngine.ini"))
        GitIgnorePath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot ".gitignore"))
    }
}

function Get-LBIniSectionContent {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Content,

        [Parameter(Mandatory = $true)]
        [string]$SectionName
    )

    $escapedSectionName = [regex]::Escape($SectionName)
    $pattern = "(?ms)^\s*\[$escapedSectionName\]\s*(?:\r?\n|`$)(.*?)(?=^\s*\[[^\]]+\]\s*(?:\r?\n|`$)|\z)"
    $match = [regex]::Match($Content, $pattern)
    if (-not $match.Success) {
        return $null
    }

    return $match.Groups[1].Value
}

function Get-EOSConfigurationDetails {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    $content = Get-Content -LiteralPath $Path -Raw -Encoding utf8
    $defaultsContent = $content
    try {
        if ([string]::Equals(
                [System.IO.Path]::GetFileName($Path),
                "GeneratedEngine.ini",
                [System.StringComparison]::OrdinalIgnoreCase)) {
            $defaultEnginePath = Join-Path (Split-Path -Parent $Path) "DefaultEngine.ini"
            if (-not (Test-Path -LiteralPath $defaultEnginePath -PathType Leaf)) {
                return $null
            }

            $defaultsContent = Get-Content -LiteralPath $defaultEnginePath -Raw -Encoding utf8
        }

        $onlineSubsystemSection = Get-LBIniSectionContent -Content $defaultsContent -SectionName "OnlineSubsystem"
        if ([string]::IsNullOrWhiteSpace($onlineSubsystemSection) -or
            $onlineSubsystemSection -cnotmatch '(?m)^\s*DefaultPlatformService\s*=\s*EOS\s*$') {
            return $null
        }

        $eosDefaultsSection = Get-LBIniSectionContent `
            -Content $defaultsContent `
            -SectionName "/Script/OnlineSubsystemEOS.EOSSettings"
        $eosArtifactSection = Get-LBIniSectionContent `
            -Content $content `
            -SectionName "/Script/OnlineSubsystemEOS.EOSSettings"
        if ([string]::IsNullOrWhiteSpace($eosDefaultsSection) -or
            [string]::IsNullOrWhiteSpace($eosArtifactSection)) {
            return $null
        }

        $defaultArtifactMatch = [regex]::Match(
            $eosDefaultsSection,
            '(?m)^\s*DefaultArtifactName\s*=\s*"?([A-Za-z][A-Za-z0-9_-]{2,63})"?\s*$')
        if (-not $defaultArtifactMatch.Success) {
            return $null
        }

        $defaultArtifact = $defaultArtifactMatch.Groups[1].Value
        $artifactMatches = [regex]::Matches(
            $eosArtifactSection,
            '(?m)^\s*\+Artifacts\s*=\s*(\([^\r\n]*\))\s*$')

        foreach ($artifactMatch in $artifactMatches) {
            $artifactEntry = $artifactMatch.Groups[1].Value
            $artifactNameMatch = [regex]::Match(
                $artifactEntry,
                '(?:^|[,(])\s*ArtifactName\s*=\s*"([A-Za-z][A-Za-z0-9_-]{2,63})"')
            if (-not $artifactNameMatch.Success -or
                -not [string]::Equals(
                    $defaultArtifact,
                    $artifactNameMatch.Groups[1].Value,
                    [System.StringComparison]::Ordinal)) {
                continue
            }

            $clientIdMatch = [regex]::Match(
                $artifactEntry,
                '(?:^|[,(])\s*ClientId\s*=\s*"(xyz[A-Za-z0-9]{20,61})"')
            $clientSecretMatch = [regex]::Match(
                $artifactEntry,
                '(?:^|[,(])\s*ClientSecret\s*=\s*"((?!\*+")[A-Za-z0-9+/_-]{20,64})"')
            $productIdMatch = [regex]::Match(
                $artifactEntry,
                '(?:^|[,(])\s*ProductId\s*=\s*"([0-9a-fA-F]{32})"')
            $sandboxIdMatch = [regex]::Match(
                $artifactEntry,
                '(?:^|[,(])\s*SandboxId\s*=\s*"(p-[a-z0-9]{20,})"')
            $deploymentIdMatch = [regex]::Match(
                $artifactEntry,
                '(?:^|[,(])\s*DeploymentId\s*=\s*"([0-9a-fA-F]{32})"')
            $clientEncryptionKeyMatch = [regex]::Match(
                $artifactEntry,
                '(?:^|[,(])\s*ClientEncryptionKey\s*=\s*"([0-9a-fA-F]{64})"')

            if ($clientIdMatch.Success -and
                $clientSecretMatch.Success -and
                $productIdMatch.Success -and
                $sandboxIdMatch.Success -and
                $deploymentIdMatch.Success -and
                $clientEncryptionKeyMatch.Success) {
                return [pscustomobject]@{
                    ArtifactName = $defaultArtifact
                    ProductId = $productIdMatch.Groups[1].Value
                }
            }
        }

        return $null
    }
    finally {
        $content = $null
        $defaultsContent = $null
    }
}

function Test-EOSConfiguration {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return $null -ne (Get-EOSConfigurationDetails -Path $Path)
}

function Assert-EOSConfiguration {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $details = Get-EOSConfigurationDetails -Path $Path
    if (-not $details) {
        throw "EOS 로컬 설정이 없거나 계약이 올바르지 않습니다. Scripts/SetupEOSDev.ps1을 직접 실행하세요. 자격값은 출력하지 않았습니다."
    }

    return $details
}

function Test-EOSCredentialEnvironment {
    $requiredNames = @(
        "EOS_PRODUCT_ID",
        "EOS_SANDBOX_ID",
        "EOS_DEPLOYMENT_ID",
        "EOS_CLIENT_ID",
        "EOS_CLIENT_SECRET"
    )

    foreach ($name in $requiredNames) {
        if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name))) {
            return $false
        }
    }

    return $true
}

function Get-UnrealEngineRoot {
    $candidates = @()
    $environmentRoot = [Environment]::GetEnvironmentVariable("UE_5_7_ROOT")
    if (-not [string]::IsNullOrWhiteSpace($environmentRoot)) {
        $candidates += $environmentRoot
    }

    $registryPaths = @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.7",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\5.7"
    )
    foreach ($registryPath in $registryPaths) {
        $registrySettings = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
        if ($registrySettings) {
            $installedDirectory = $registrySettings.PSObject.Properties["InstalledDirectory"]
            if ($installedDirectory -and -not [string]::IsNullOrWhiteSpace($installedDirectory.Value)) {
                $candidates += $installedDirectory.Value
            }
        }
    }

    $sourceBuilds = Get-ItemProperty -LiteralPath "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue
    if ($sourceBuilds) {
        foreach ($property in $sourceBuilds.PSObject.Properties) {
            if (-not $property.Name.StartsWith("PS") -and
                $property.Value -is [string] -and
                -not [string]::IsNullOrWhiteSpace($property.Value)) {
                $candidates += $property.Value
            }
        }
    }

    $programData = [Environment]::GetEnvironmentVariable("ProgramData")
    if (-not [string]::IsNullOrWhiteSpace($programData)) {
        $launcherInstalledPath = Join-Path $programData "Epic\UnrealEngineLauncher\LauncherInstalled.dat"
        if (Test-Path -LiteralPath $launcherInstalledPath -PathType Leaf) {
            try {
                $launcherManifest = Get-Content -LiteralPath $launcherInstalledPath -Raw -Encoding utf8 | ConvertFrom-Json
                foreach ($installation in @($launcherManifest.InstallationList)) {
                    if ($installation.AppName -eq "UE_5.7" -or
                        $installation.ArtifactId -eq "UE_5.7") {
                        $candidates += $installation.InstallLocation
                    }
                }
            }
            catch {
                # A damaged Launcher manifest is ignored; registry/default discovery continues.
            }
        }
    }

    $programFiles = [Environment]::GetEnvironmentVariable("ProgramFiles")
    if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
        $candidates += (Join-Path $programFiles "Epic Games\UE_5.7")
    }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $buildVersionPath = Join-Path $candidate "Engine\Build\Build.version"
        $runUATPath = Join-Path $candidate "Engine\Build\BatchFiles\RunUAT.bat"
        if (-not (Test-Path -LiteralPath $buildVersionPath -PathType Leaf) -or
            -not (Test-Path -LiteralPath $runUATPath -PathType Leaf)) {
            continue
        }

        try {
            $buildVersion = Get-Content -LiteralPath $buildVersionPath -Raw -Encoding utf8 | ConvertFrom-Json
            if ([int]$buildVersion.MajorVersion -eq 5 -and
                [int]$buildVersion.MinorVersion -eq 7 -and
                [int]$buildVersion.PatchVersion -eq 4) {
                return [System.IO.Path]::GetFullPath($candidate)
            }
        }
        catch {
            continue
        }
    }

    throw "Unreal Engine 5.7.4 설치 경로를 찾을 수 없습니다. UE_5_7_ROOT 환경 변수 또는 Epic Games Launcher 설치를 확인하세요."
}

function Test-LBEpicSignature {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }

    try {
        $signature = Get-AuthenticodeSignature -LiteralPath $Path
        return $signature.Status -eq [System.Management.Automation.SignatureStatus]::Valid -and
            $signature.SignerCertificate -and
            $signature.SignerCertificate.Subject -like '*O=Epic Games Inc.*'
    }
    catch {
        return $false
    }
}

function Get-LBUnrealEngineInfo {
    $engineRoot = Get-UnrealEngineRoot
    $editorPath = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
    $editorCmdPath = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    $runUATPath = Join-Path $engineRoot "Engine\Build\BatchFiles\RunUAT.bat"
    $eosSdkPath = Join-Path $engineRoot "Engine\Binaries\Win64\EOSSDK-Win64-Shipping.dll"

    foreach ($requiredPath in @($editorPath, $editorCmdPath, $runUATPath, $eosSdkPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Unreal Engine 5.7.4 필수 파일이 없습니다: $requiredPath"
        }
    }

    if (-not (Test-LBEpicSignature -Path $eosSdkPath)) {
        throw "UE 5.7.4 EOS SDK DLL의 Epic Games 디지털 서명이 유효하지 않습니다. 엔진 설치를 복구하세요."
    }

    return [pscustomobject]@{
        Root = $engineRoot
        EditorPath = $editorPath
        EditorCmdPath = $editorCmdPath
        RunUATPath = $runUATPath
        EOSSDKPath = $eosSdkPath
    }
}

function Resolve-SignedEpicBinary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EnvironmentName,

        [Parameter(Mandatory = $true)]
        [string[]]$Candidates,

        [Parameter(Mandatory = $true)]
        [string]$DisplayName,

        [Parameter(Mandatory = $true)]
        [string[]]$ExpectedFileNames,

        [string]$ExpectedProductName,

        [string]$ExpectedFileDescription
    )

    $paths = @()
    $environmentPath = [Environment]::GetEnvironmentVariable($EnvironmentName)
    if (-not [string]::IsNullOrWhiteSpace($environmentPath)) {
        $paths += $environmentPath
    }
    $paths += $Candidates

    foreach ($path in ($paths | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }

        $leafName = [System.IO.Path]::GetFileName($path)
        if ($ExpectedFileNames -notcontains $leafName -or
            -not (Test-LBEpicSignature -Path $path)) {
            continue
        }

        $file = Get-Item -LiteralPath $path
        if (-not [string]::IsNullOrWhiteSpace($ExpectedProductName) -and
            -not [string]::Equals(
                $file.VersionInfo.ProductName,
                $ExpectedProductName,
                [System.StringComparison]::Ordinal)) {
            continue
        }
        if (-not [string]::IsNullOrWhiteSpace($ExpectedFileDescription) -and
            -not [string]::Equals(
                $file.VersionInfo.FileDescription,
                $ExpectedFileDescription,
                [System.StringComparison]::Ordinal)) {
            continue
        }

        return [System.IO.Path]::GetFullPath($path)
    }

    throw "$DisplayName 파일을 찾지 못했거나 Epic Games 디지털 서명이 유효하지 않습니다. $EnvironmentName 환경 변수로 공식 EOS 배포 파일 경로를 지정하세요."
}

function Get-EOSBootstrapperCandidates {
    $candidates = @()
    $programData = [Environment]::GetEnvironmentVariable("ProgramData")
    $programFiles = [Environment]::GetEnvironmentVariable("ProgramFiles")
    $programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")

    if (-not [string]::IsNullOrWhiteSpace($programData)) {
        $candidates += (Join-Path $programData "Epic\EpicGamesLauncher\Data\Update\Install\Portal\Extras\EOSBootStrapper\EOSBootStrapper.exe")
    }
    if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
        $candidates += (Join-Path $programFiles "Epic Games\Launcher\Portal\Extras\EOSBootStrapper\EOSBootStrapper.exe")
    }
    if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
        $candidates += (Join-Path $programFilesX86 "Epic Games\Launcher\Portal\Extras\EOSBootStrapper\EOSBootStrapper.exe")
    }

    return $candidates
}

function Get-EOSRedistributableInstallerCandidates {
    $candidates = @()
    $programData = [Environment]::GetEnvironmentVariable("ProgramData")
    $programFiles = [Environment]::GetEnvironmentVariable("ProgramFiles")
    $programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")

    if (-not [string]::IsNullOrWhiteSpace($programData)) {
        $candidates += (Join-Path $programData "Epic\EpicGamesLauncher\Data\Update\Install\Portal\Extras\EOS\EpicOnlineServicesInstaller.exe")
    }
    if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
        $candidates += (Join-Path $programFiles "Epic Games\Launcher\Portal\Extras\EOS\EpicOnlineServicesInstaller.exe")
    }
    if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
        $candidates += (Join-Path $programFilesX86 "Epic Games\Launcher\Portal\Extras\EOS\EpicOnlineServicesInstaller.exe")
    }

    return $candidates
}

function Assert-LBGeneratedConfigIgnored {
    param(
        [Parameter(Mandatory = $true)]
        [psobject]$ProjectContext
    )

    if (-not (Test-Path -LiteralPath $ProjectContext.GitIgnorePath -PathType Leaf)) {
        throw ".gitignore 파일이 없어 EOS 로컬 자격 파일을 안전하게 생성할 수 없습니다."
    }

    $git = Get-Command git -ErrorAction Stop
    $gitSafeRoot = $ProjectContext.ProjectRoot.Replace('\', '/')
    $gitArguments = @(
        '-c',
        "safe.directory=$gitSafeRoot",
        '-C',
        $ProjectContext.ProjectRoot
    )

    foreach ($relativePath in @(
            "Config/GeneratedEngine.ini",
            "Config/GeneratedEngine.ini.tmp")) {
        $trackedMatches = @(& $git.Source @gitArguments ls-files -- $relativePath)
        if ($LASTEXITCODE -ne 0) {
            throw "Git 추적 상태를 확인하지 못했습니다. 자격 파일을 생성하지 않았습니다."
        }
        if ($trackedMatches.Count -gt 0) {
            throw "$relativePath 파일이 이미 Git에 추적되고 있어 자격값을 쓸 수 없습니다. 먼저 저장소 관리자와 추적 및 노출 여부를 확인하세요."
        }

        & $git.Source @gitArguments check-ignore --no-index -q -- $relativePath
        if ($LASTEXITCODE -ne 0) {
            throw ".gitignore의 유효 규칙이 $relativePath 파일을 제외하지 않습니다. 자격 파일을 생성하지 않았습니다."
        }
    }

    return $true
}
