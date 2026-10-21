"""Build the pinned shared GEOS SDK and retain its license for wheel packaging."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request


VERSION = "3.15.0"
SHA256 = "d5e5192a686d065eaed082de14dd26244c5c8e02bff16b2c6cce3265f648e00e"


def main():
    root = Path(__file__).resolve().parents[1]
    deps = root / "build/deps"
    deps.mkdir(parents=True, exist_ok=True)
    archive = deps / f"geos-{VERSION}.tar.bz2"
    if not archive.exists():
        partial = archive.with_suffix(".partial")
        try:
            urllib.request.urlretrieve(f"https://download.osgeo.org/geos/geos-{VERSION}.tar.bz2", partial)
            if hashlib.sha256(partial.read_bytes()).hexdigest() != SHA256:
                raise RuntimeError("GEOS source checksum mismatch")
            partial.replace(archive)
        finally:
            partial.unlink(missing_ok=True)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise RuntimeError("GEOS source checksum mismatch")
    source = deps / f"geos-{VERSION}"
    if not source.exists():
        with tarfile.open(archive) as package:
            package.extractall(deps, filter="data")
    sdk, build = deps / "geos-sdk", deps / "geos-build"
    cmake = shutil.which("cmake") or str(root / (".venv/Scripts/cmake.exe" if sys.platform == "win32" else ".venv/bin/cmake"))
    command = [
        cmake, "-S", str(source), "-B", str(build),
        "-DBUILD_TESTING=OFF", "-DBUILD_DOCUMENTATION=OFF", "-DBUILD_GEOSOP=OFF",
        "-DBUILD_SHARED_LIBS=ON", f"-DCMAKE_INSTALL_PREFIX={sdk}", "-DCMAKE_BUILD_TYPE=Release",
    ]
    if sys.platform == "win32":
        command += ["-G", "Visual Studio 17 2022", "-A", "x64", "-DCMAKE_CXX_FLAGS=/MP8 /EHsc /utf-8"]
    subprocess.run(command, check=True)
    subprocess.run([cmake, "--build", str(build), "--config", "Release", "--parallel", "8"], check=True)
    subprocess.run([cmake, "--install", str(build), "--config", "Release"], check=True)
    license_dir = sdk / "share/licenses/geos"
    license_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / "COPYING", license_dir / "COPYING")
    print(f"GEOS_ROOT={sdk}")


if __name__ == "__main__":
    main()
