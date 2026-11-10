"""Removal must preserve user files and refuse bad receipts before any deletion."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import pytest

pytestmark=pytest.mark.skipif(os.name!='nt',reason='Windows native portable-install removal')
SCRIPT=Path(__file__).parents[1]/'tools/ci/remove_native_handoff.ps1'


@pytest.mark.parametrize('mode',['modified_member','outside_prefix','valid'])
def test_removal_preflights_all_members_and_preserves_user_data(tmp_path,mode):
    install=tmp_path/'installed/native app';install.mkdir(parents=True)
    a,b=install/'a.exe',install/'b.dll'
    a.write_bytes(b'authored stand-in A');b.write_bytes(b'authored stand-in B')
    user=install/'research notes.txt';user.write_bytes(b'owned research notes')
    outside=tmp_path/'outside.dat';outside.write_bytes(b'not a package member')
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    members={str(a):digest(a),str(b):digest(b)}
    if mode=='modified_member':b.write_bytes(b'changed after installation')
    if mode=='outside_prefix':members[str(outside)]=digest(outside)
    receipt=tmp_path/'handoff.json'
    receipt.write_text(json.dumps({'state':'install_and_use_passed','work':str(tmp_path),
        'install_run_verified':True,'uninstall_verified':False,'installed_files':members,
        'protected_user_files':{str(user):digest(user)}}),encoding='utf8')
    env=os.environ.copy()
    env.pop('PSMODULEPATH',None)  # Windows os.environ copies use uppercase keys; avoid inherited PS7 modules.
    result=subprocess.run(['powershell.exe','-NoProfile','-NonInteractive','-File',str(SCRIPT),
                           '-Receipt',str(receipt)],capture_output=True,env=env)
    assert user.read_bytes()==b'owned research notes' and outside.read_bytes()==b'not a package member'
    if mode=='valid':
        assert result.returncode==0,(result.stdout,result.stderr)
        assert not a.exists() and not b.exists()
        assert json.loads(receipt.read_text(encoding='utf-8-sig'))['uninstall_verified']
    else:
        assert result.returncode!=0 and a.exists() and b.exists()
        reason='Modified install member' if mode=='modified_member' else 'Install member outside owned prefix'
        assert reason in (result.stdout+result.stderr).decode(errors='replace')
        assert not json.loads(receipt.read_text())['uninstall_verified']
