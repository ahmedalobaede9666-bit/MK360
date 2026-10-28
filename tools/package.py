#!/usr/bin/env python3
"""Create the user package after make's native and XEX checks have passed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
from verify_xex import verify

ROOT=Path(__file__).resolve().parents[1]
VERSION="0.1.0-experimental"
def digest(data):return hashlib.sha256(data).hexdigest()
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--compiler",default="powerpc-xsdk-elf-gcc")
    args=parser.parse_args()
    binary=(ROOT/"build/MK360.xex").read_bytes()
    result=verify(binary)
    core_result=(ROOT/"build/core-test.log").read_text().strip()
    if not core_result.startswith("PASS:"):raise SystemExit("native test result is missing")
    compiler=subprocess.check_output([args.compiler,"--version"],text=True).splitlines()[0]
    info={"project":"MK360","version":VERSION,"source":"https://github.com/ahmedalobaede9666-bit/MK360",
          "source_revision":os.environ.get("GITHUB_SHA","working-tree"),"compiler":compiler,
          "xdk_required":False,"hardware_tested":False,"kernel":17559,
          "native_validation":core_result,"xex_validation":result,"xex_sha256":digest(binary)}
    files={"MK360/MK360.xex":binary,"MK360/MK360.ini":(ROOT/"config/MK360.ini").read_bytes(),
           "README_AR.md":(ROOT/"docs/README_AR.md").read_bytes(),
           "HARDWARE_STATUS.md":(ROOT/"docs/HARDWARE_STATUS.md").read_bytes(),
           "launch.example.ini":(ROOT/"config/launch.example.ini").read_bytes(),
           "LICENSE":(ROOT/"LICENSE").read_bytes(),
           "THIRD_PARTY_NOTICES.md":(ROOT/"THIRD_PARTY_NOTICES.md").read_bytes(),
           "licenses/xsdk-MIT.txt":(ROOT/"third_party/licenses/xsdk-MIT.txt").read_bytes(),
           "BUILDINFO.json":(json.dumps(info,ensure_ascii=False,indent=2)+"\n").encode()}
    files["SHA256SUMS.txt"]="".join(f"{digest(data)}  {name}\n" for name,data in sorted(files.items())).encode()
    out=ROOT/"dist";out.mkdir(exist_ok=True)
    archive=out/f"MK360-{VERSION}.zip"
    with zipfile.ZipFile(archive,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in sorted(files.items()):
            item=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0));item.compress_type=zipfile.ZIP_DEFLATED
            item.external_attr=0o100644<<16;z.writestr(item,data)
    shutil.copyfile(ROOT/"build/MK360.xex",out/"MK360.xex")
    (out/"BUILDINFO.json").write_bytes(files["BUILDINFO.json"])
    (out/"SHA256SUMS.txt").write_text("".join(f"{digest(p.read_bytes())}  {p.name}\n" for p in sorted(out.iterdir()) if p.name!="SHA256SUMS.txt"))
    print(f"Packaged {archive.name}; XEX SHA-256 {digest(binary)}")

if __name__=="__main__":main()
