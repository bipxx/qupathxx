# SPDX-License-Identifier: AGPL-3.0-only
param(
    [Parameter(Mandatory)][string] $Archives,
    [Parameter(Mandatory)][string] $BuildRoot,
    [string] $Cmake = 'cmake',
    [ValidateRange(1, 64)][int] $Jobs = 6
)

$ErrorActionPreference = 'Stop'
$pins = Get-Content "$PSScriptRoot/../../plugins/parquet_matrix/decoder-sources.json" -Raw | ConvertFrom-Json -AsHashtable
$root = (New-Item -ItemType Directory -Force -Path $BuildRoot).FullName.Replace('\', '/')
$inputs = (Resolve-Path -LiteralPath $Archives).Path.Replace('\', '/')
$prior = @{}
try {
    foreach ($pin in $pins.GetEnumerator()) {
        $file = "$inputs/$($pin.Key)"
        if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.Value.sha256) {
            throw "Dependency differs from its pin: $($pin.Key)"
        }
        if ($pin.Value.environment) {
            $name = $pin.Value.environment
            $prior[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
            [Environment]::SetEnvironmentVariable($name, $file, 'Process')
        }
    }
    $release = 'apache-arrow-26.0.1'
    & tar.exe -xf "$inputs/$release.tar.gz" -C $root "$release/cpp" "$release/format" "$release/.env" "$release/LICENSE.txt" "$release/NOTICE.txt"
    if ($LASTEXITCODE) { throw 'Unable to extract the verified Arrow release' }
    & $Cmake -S "$root/$release/cpp" -B "$root/decoder-cache" -G 'Visual Studio 17 2022' -A x64 `
        "-DCMAKE_INSTALL_PREFIX=$root/decoder-sdk" -DARROW_DEPENDENCY_SOURCE=BUNDLED -DARROW_BUILD_STATIC=ON `
        -DARROW_BUILD_SHARED=OFF -DARROW_PARQUET=ON -DARROW_COMPUTE=OFF -DARROW_MIMALLOC=OFF -DARROW_JEMALLOC=OFF `
        -DARROW_SIMD_LEVEL=NONE -DARROW_RUNTIME_SIMD_LEVEL=NONE -DARROW_WITH_SNAPPY=ON -DARROW_WITH_ZSTD=ON `
        -DPARQUET_REQUIRE_ENCRYPTION=OFF -DARROW_BUILD_TESTS=OFF -DARROW_BUILD_UTILITIES=OFF `
        -DARROW_PACKAGE_KIND=qupathxx-parquet
    if ($LASTEXITCODE) { throw 'Arrow/Parquet configuration failed' }
    & $Cmake --build "$root/decoder-cache" --config Release --target INSTALL --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Arrow/Parquet build or installation failed' }
} finally {
    foreach ($saved in $prior.GetEnumerator()) {
        [Environment]::SetEnvironmentVariable($saved.Key, $saved.Value, 'Process')
    }
}
