[CmdletBinding()]
param(
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration = "Shipping",

    [string]$OutputRoot,

    [switch]$AllowEditorRunning
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $projectRoot "LeftBehind.uproject"
$generatedConfigPath = Join-Path $projectRoot "Config\GeneratedEngine.ini"
$setupScriptPath = Join-Path $PSScriptRoot "SetupEOSDev.ps1"

function Get-EOSConfigurationDetails {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    $content = Get-Content -LiteralPath $Path -Raw -Encoding utf8
    $defaultArtifactMatch = [regex]::Match(
        $content,
        '(?m)^\s*DefaultArtifactName\s*=\s*"([A-Za-z][A-Za-z0-9_-]{2,63})"\s*$')
    if (-not $defaultArtifactMatch.Success) {
        return $null
    }

    $defaultArtifact = $defaultArtifactMatch.Groups[1].Value
    $artifactMatches = [regex]::Matches(
        $content,
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
            '(?:^|[,(])\s*ClientId\s*=\s*"(xyz[A-Za-z0-9]{20,})"')
        $clientSecretMatch = [regex]::Match(
            $artifactEntry,
            '(?:^|[,(])\s*ClientSecret\s*=\s*"((?!\*+")[A-Za-z0-9+/_-]{20,})"')
        $productIdMatch = [regex]::Match(
            $artifactEntry,
            '(?:^|[,(])\s*ProductId\s*=\s*"([0-9a-fA-F]{32})"')
        $sandboxIdMatch = [regex]::Match(
            $artifactEntry,
            '(?:^|[,(])\s*SandboxId\s*=\s*"(p-[a-z0-9]{20,})"')
        $deploymentIdMatch = [regex]::Match(
            $artifactEntry,
            '(?:^|[,(])\s*DeploymentId\s*=\s*"([0-9a-fA-F]{32})"')

        if ($clientIdMatch.Success -and
            $clientSecretMatch.Success -and
            $productIdMatch.Success -and
            $sandboxIdMatch.Success -and
            $deploymentIdMatch.Success) {
            return [pscustomobject]@{
                ArtifactName = $defaultArtifact
                ProductId = $productIdMatch.Groups[1].Value
            }
        }
    }

    return $null
}

function Test-EOSConfiguration {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return $null -ne (Get-EOSConfigurationDetails -Path $Path)
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

    $registryPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.7"
    $registrySettings = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
    if ($registrySettings) {
        $installedDirectory = $registrySettings.PSObject.Properties["InstalledDirectory"]
        if ($installedDirectory -and -not [string]::IsNullOrWhiteSpace($installedDirectory.Value)) {
            $candidates += $installedDirectory.Value
        }
    }

    $programFiles = [Environment]::GetEnvironmentVariable("ProgramFiles")
    if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
        $candidates += (Join-Path $programFiles "Epic Games\UE_5.7")
    }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        $runUAT = Join-Path $candidate "Engine\Build\BatchFiles\RunUAT.bat"
        if (Test-Path -LiteralPath $runUAT -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    throw "Unreal Engine 5.7 설치 경로를 찾을 수 없습니다. UE_5_7_ROOT 환경 변수 또는 Epic Games Launcher 설치를 확인하세요."
}

function Resolve-SignedEpicBinary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EnvironmentName,

        [Parameter(Mandatory = $true)]
        [string[]]$Candidates,

        [Parameter(Mandatory = $true)]
        [string]$DisplayName
    )

    $paths = @()
    $environmentPath = [Environment]::GetEnvironmentVariable($EnvironmentName)
    if (-not [string]::IsNullOrWhiteSpace($environmentPath)) {
        $paths += $environmentPath
    }
    $paths += $Candidates

    foreach ($path in ($paths | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            continue
        }

        $signature = Get-AuthenticodeSignature -LiteralPath $path
        if ($signature.Status -eq [System.Management.Automation.SignatureStatus]::Valid -and
            $signature.SignerCertificate -and
            $signature.SignerCertificate.Subject -like '*O=Epic Games Inc.*') {
            return [System.IO.Path]::GetFullPath($path)
        }
    }

    throw "$DisplayName 파일을 찾지 못했거나 Epic Games 디지털 서명이 유효하지 않습니다. $EnvironmentName 환경 변수로 공식 EOS 배포 파일 경로를 지정하세요."
}

if ((Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue) -and -not $AllowEditorRunning) {
    throw "패키징 중 DLL 잠금을 방지하려면 실행 중인 Unreal Editor를 모두 종료해야 합니다."
}
if ($AllowEditorRunning -and (Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue)) {
    Write-Warning "Unreal Editor가 실행 중입니다. 저장되지 않은 에셋 변경은 패키지에 포함되지 않습니다."
}

if (-not (Test-EOSConfiguration -Path $generatedConfigPath)) {
    if (Test-EOSCredentialEnvironment) {
        & $setupScriptPath -NonInteractive
    }
    else {
        throw "빌드 PC의 EOS 설정이 없습니다. Scripts/SetupEOSDev.ps1을 한 번 실행하거나 EOS_* secret 환경 변수를 설정하세요. 다른 PC에서는 이 작업이 필요하지 않습니다."
    }
}

if (-not (Test-EOSConfiguration -Path $generatedConfigPath)) {
    throw "EOS 설정 검증에 실패했습니다. 자격값은 출력하지 않았습니다."
}
$eosConfiguration = Get-EOSConfigurationDetails -Path $generatedConfigPath

if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
    throw "Unreal 프로젝트를 찾을 수 없습니다: $projectPath"
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $projectRoot "Output"
}
elseif (-not [System.IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot = Join-Path $projectRoot $OutputRoot
}

$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$archiveDirectory = Join-Path $OutputRoot "LeftBehind-Win64-$Configuration-$timestamp"
$engineRoot = Get-UnrealEngineRoot
$runUATPath = Join-Path $engineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$programData = [Environment]::GetEnvironmentVariable("ProgramData")
$programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
$bootstrapperCandidates = @()
$installerCandidates = @()
if (-not [string]::IsNullOrWhiteSpace($programData)) {
    $bootstrapperCandidates += (Join-Path $programData "Epic\EpicGamesLauncher\Data\Update\Install\Portal\Extras\EOSBootStrapper\EOSBootStrapper.exe")
    $installerCandidates += (Join-Path $programData "Epic\EpicGamesLauncher\Data\Update\Install\Portal\Extras\EOS\EpicOnlineServicesInstaller.exe")
}
if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
    $bootstrapperCandidates += (Join-Path $programFilesX86 "Epic Games\Launcher\Portal\Extras\EOSBootStrapper\EOSBootStrapper.exe")
    $installerCandidates += (Join-Path $programFilesX86 "Epic Games\Launcher\Portal\Extras\EOS\EpicOnlineServicesInstaller.exe")
}
$eosBootstrapperSource = Resolve-SignedEpicBinary `
    -EnvironmentName "EOS_BOOTSTRAPPER_PATH" `
    -Candidates $bootstrapperCandidates `
    -DisplayName "EOS Bootstrapper"
$eosInstallerSource = Resolve-SignedEpicBinary `
    -EnvironmentName "EOS_REDISTRIBUTABLE_INSTALLER_PATH" `
    -Candidates $installerCandidates `
    -DisplayName "EOS Redistributable Installer"

New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

Write-Host "EOS 설정 검증 완료. 자격값은 출력하지 않습니다." -ForegroundColor Green
Write-Host "Win64 $Configuration 무설정 배포 패키지를 생성합니다: $archiveDirectory"

$uatArguments = @(
    "BuildCookRun",
    "-project=$projectPath",
    "-target=LeftBehind",
    "-noP4",
    "-utf8output",
    "-unattended",
    "-platform=Win64",
    "-clientconfig=$Configuration",
    "-build",
    "-nocompileeditor",
    "-cook",
    "-stage",
    "-package",
    "-pak",
    "-iostore",
    "-compressed",
    "-prereqs",
    "-archive",
    "-archivedirectory=$archiveDirectory"
)
if ($Configuration -eq "Shipping") {
    $uatArguments += "-nodebuginfo"
}

& $runUATPath @uatArguments
if ($LASTEXITCODE -ne 0) {
    throw "Unreal AutomationTool 패키징이 실패했습니다. 종료 코드: $LASTEXITCODE"
}

$stagedManifest = Get-ChildItem `
    -LiteralPath (Join-Path $projectRoot "Saved\StagedBuilds") `
    -Recurse `
    -File `
    -Filter "Manifest_UFSFiles_Win64.txt" `
    -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $stagedManifest) {
    throw "배포 검증 실패: Win64 UFS 스테이징 매니페스트가 없습니다."
}

$stagedManifestContent = Get-Content -LiteralPath $stagedManifest.FullName -Raw
$requiredCookedUIAssets = @(
    "LeftBehind/Content/LeftBehind/UI/MainMenu/WBP_MainMenu.uasset",
    "LeftBehind/Content/LeftBehind/UI/MainMenu/WBP_CodenameEntry.uasset",
    "LeftBehind/Content/LeftBehind/UI/MainMenu/WBP_WaitingRoom.uasset",
    "LeftBehind/Content/LeftBehind/UI/CharacterSelect/WBP_LB_CharacterSelectWidget.uasset",
    "LeftBehind/Content/LeftBehind/UI/BattleHUD/HUD/WBP_LB_RaidHUDWidget.uasset"
)
foreach ($requiredCookedUIAsset in $requiredCookedUIAssets) {
    if (-not $stagedManifestContent.Contains($requiredCookedUIAsset)) {
        throw "배포 검증 실패: 필수 UI 에셋이 cook/stage되지 않았습니다: $requiredCookedUIAsset"
    }
}

$gameExecutable = Get-ChildItem -LiteralPath $archiveDirectory -Recurse -File -Filter "LeftBehind.exe" |
    Select-Object -First 1
$eosSdk = Get-ChildItem -LiteralPath $archiveDirectory -Recurse -File -Filter "EOSSDK-Win64-Shipping.dll" |
    Select-Object -First 1
$pakFile = Get-ChildItem -LiteralPath $archiveDirectory -Recurse -File -Filter "*.pak" |
    Select-Object -First 1
$ioStoreFile = Get-ChildItem -LiteralPath $archiveDirectory -Recurse -File -Filter "*.utoc" |
    Select-Object -First 1
$prerequisiteInstaller = Get-ChildItem -LiteralPath $archiveDirectory -Recurse -File -Filter "vc_redist.x64.exe" |
    Select-Object -First 1

if (-not $gameExecutable) {
    throw "배포 검증 실패: LeftBehind.exe가 없습니다."
}
if (-not $eosSdk) {
    throw "배포 검증 실패: EOSSDK-Win64-Shipping.dll이 없습니다."
}
if (-not $pakFile -or -not $ioStoreFile) {
    throw "배포 검증 실패: pak 또는 IoStore 컨테이너가 없습니다."
}
if (-not $prerequisiteInstaller) {
    throw "배포 검증 실패: vc_redist.x64.exe가 없습니다."
}

$gameRoot = $gameExecutable.Directory.FullName
$installerDirectory = Join-Path $gameRoot "Installers"
$packagedBootstrapper = Join-Path $gameRoot "EOSBootstrapper.exe"
$packagedBootstrapperConfig = Join-Path $gameRoot "EOSBootstrapper.ini"
$packagedEOSInstaller = Join-Path $installerDirectory "EpicOnlineServicesInstaller.exe"
$launcherPath = Join-Path $gameRoot "StartLeftBehind.cmd"
try {
    $vcRedistVersion = ([version]$prerequisiteInstaller.VersionInfo.ProductVersion).ToString()
}
catch {
    throw "배포 검증 실패: 포함된 VC++ 필수 구성 요소의 버전을 확인할 수 없습니다."
}

New-Item -ItemType Directory -Path $installerDirectory -Force | Out-Null
Copy-Item -LiteralPath $eosBootstrapperSource -Destination $packagedBootstrapper -Force
Copy-Item -LiteralPath $eosInstallerSource -Destination $packagedEOSInstaller -Force

$utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
$bootstrapperConfig = @"
ApplicationPath=./LeftBehind.exe
WorkingDirectory=.
WaitForExit=0
NoOperation=0
"@
[System.IO.File]::WriteAllText($packagedBootstrapperConfig, $bootstrapperConfig, $utf8WithoutBom)

$launcher = @"
@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "VC_REQUIRED_VERSION=$vcRedistVersion"
call :vc_runtime_is_current
if not errorlevel 1 goto vc_ready

start /wait "" "%~dp0Engine\Extras\Redist\en-us\vc_redist.x64.exe" /install /quiet /norestart
set "VC_INSTALL_EXIT=!ERRORLEVEL!"
call :vc_runtime_is_current
if errorlevel 1 (
    echo Failed to install the required Microsoft Visual C++ Runtime.
    if "!VC_INSTALL_EXIT!"=="0" exit /b 1
    exit /b !VC_INSTALL_EXIT!
)
if "!VC_INSTALL_EXIT!"=="3010" (
    echo Windows must be restarted before launching Left Behind.
    pause
    exit /b 0
)

:vc_ready

set "EOS_MARKER=%LOCALAPPDATA%\LeftBehind\EOS-$($eosConfiguration.ProductId).installed"
sc.exe query EpicOnlineServices >nul 2>&1
if errorlevel 1 goto install_eos
if not exist "%EOS_MARKER%" goto install_eos
goto launch_game

:install_eos
if not exist "%LOCALAPPDATA%\LeftBehind" mkdir "%LOCALAPPDATA%\LeftBehind"
start /wait "" "%~dp0Installers\EpicOnlineServicesInstaller.exe" /install productId=$($eosConfiguration.ProductId)
set "EOS_INSTALL_EXIT=!ERRORLEVEL!"
if "!EOS_INSTALL_EXIT!"=="3010" (
    echo Windows must be restarted before launching Left Behind.
    pause
    exit /b 0
)
if not "!EOS_INSTALL_EXIT!"=="0" exit /b !EOS_INSTALL_EXIT!
>"%EOS_MARKER%" echo installed

:launch_game
start "" "%~dp0EOSBootstrapper.exe"
exit /b 0

:vc_runtime_is_current
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "`$ErrorActionPreference='SilentlyContinue'; `$runtime=Get-ItemProperty -LiteralPath 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64'; if (`$runtime.Installed -eq 1 -and [version]([string]`$runtime.Version).TrimStart('v') -ge [version]'%VC_REQUIRED_VERSION%') { exit 0 }; exit 1"
exit /b %ERRORLEVEL%
"@
[System.IO.File]::WriteAllText($launcherPath, $launcher, [System.Text.Encoding]::ASCII)

$packagedBootstrapperSignature = Get-AuthenticodeSignature -LiteralPath $packagedBootstrapper
$packagedInstallerSignature = Get-AuthenticodeSignature -LiteralPath $packagedEOSInstaller
if ($packagedBootstrapperSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
    $packagedInstallerSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
    throw "배포 검증 실패: 복사된 EOS 배포 파일의 디지털 서명이 유효하지 않습니다."
}

$instructionsPath = Join-Path $gameRoot "실행방법.txt"
$instructions = @"
LEFT BEHIND Win64 $Configuration

1. 이 폴더 전체를 다른 PC로 복사합니다. 파일 일부만 복사하면 EOS가 동작하지 않습니다.
2. StartLeftBehind.cmd를 실행합니다.
3. 최초 실행에만 Windows 관리자 권한 확인이 한 번 이상 나타날 수 있습니다. 승인하면 VC++ 필수 구성 요소와 공식 Epic Online Services Redistributable이 자동 설치됩니다.
4. 이후 Epic 계정으로 로그인합니다.

EOS Artifact 설정, EOS SDK, Bootstrapper와 Redistributable Installer가 패키지에 포함되어 있으므로 대상 PC에서 프로젝트 설정이나 SetupEOSDev.ps1을 실행할 필요가 없습니다.
"@
$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText($instructionsPath, $instructions, $utf8WithBom)

Write-Host "무설정 EOS 배포 패키지 검증 완료." -ForegroundColor Green
Write-Host "전달할 폴더: $gameRoot"
