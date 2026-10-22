"""Native streaming file observations, stable identity, quotas and atomic JSON."""
import gc
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time

import pytest
import qupathxx as q


@pytest.mark.parametrize("size",[0,1,3,65535,65536,65537,1048576,1048577])
def test_native_digest_matches_independent_hashlib_for_chunk_edges(tmp_path,size):
    data=(bytes(range(256))*((size+255)//256))[:size];path=tmp_path/"\u96ea.bin";path.write_bytes(data)
    options=q.FileFingerprintOptions();options.buffer_bytes=65536;options.byte_limit=size
    value=q.fingerprint_file(str(path),options)
    assert value.bytes==size and value.sha256==hashlib.sha256(data).hexdigest() and value.path==str(path.resolve())
    assert value.identity and value.modified_token and value.change_token and value.elapsed_ms>=0
    assert value.identity_scheme==("windows.volume-file-id128/1" if os.name=="nt" else "posix.device-inode/1")
    before=q.file_fingerprint_json(value);parsed=json.loads(before)
    assert parsed["schema"]=="qpx.file-fingerprint/1" and parsed["bytes"]==size and parsed["sha256"]==value.sha256
    assert parsed["path"]==value.path and parsed["identity"]==value.identity and parsed["modified_token"]==value.modified_token and parsed["change_token"]==value.change_token
    assert parsed["elapsed_ms"]==value.elapsed_ms and parsed["started_unix_ms"]==value.started_unix_ms and value.finished_unix_ms>0
    with pytest.raises(AttributeError):value.sha256="changed"
    path.write_bytes(b"changed");assert q.file_fingerprint_json(value)==before


@pytest.mark.parametrize("attribute,value",[("byte_limit",2**63),("buffer_bytes",0),("buffer_bytes",65535),("buffer_bytes",16*1024*1024+1)])
def test_invalid_work_limits_reject_before_launch(tmp_path,attribute,value):
    options=q.FileFingerprintOptions();setattr(options,attribute,value);job=q.FileFingerprintJob()
    with pytest.raises(ValueError):job.request(str(tmp_path/"missing"),options=options)
    assert job.poll().state=="idle" and not job.busy and job.result is None


@pytest.mark.parametrize("path",["","bad\0name"])
def test_invalid_paths_reject_before_launch(path):
    job=q.FileFingerprintJob()
    with pytest.raises(ValueError):job.request(path)
    assert not job.busy


def test_worker_failure_preserves_destination_and_can_be_reused(tmp_path):
    source=tmp_path/"source.bin";source.write_bytes(b"abc");output=tmp_path/"\u6765\u6e90.json";output.write_bytes(b"sentinel")
    options=q.FileFingerprintOptions();options.byte_limit=2;job=q.FileFingerprintJob()
    assert job.request(str(source),str(output),options);status=job.wait()
    assert status.state=="failed" and "byte limit" in status.error and status.total_bytes==3 and status.completed==0 and job.result is None
    assert output.read_bytes()==b"sentinel" and not list(tmp_path.glob("*.qpx-tmp-*"))
    options.byte_limit=3;assert job.request(str(source),str(output),options);status=job.wait();value=job.result
    assert status.state=="succeeded" and status.bytes==status.total_bytes==3 and status.completed==1
    assert output.read_bytes()==q.file_fingerprint_json(value).encode() and status.elapsed_ms>=value.elapsed_ms
    source.write_bytes(b"new content");assert value.sha256==hashlib.sha256(b"abc").hexdigest()
    assert job.request(str(tmp_path/"missing"));assert job.wait().state=="failed" and job.result is None and value.bytes==3
    assert job.request(str(tmp_path));assert job.wait().state=="failed"
    job.cancel();assert not job.busy


def test_input_aliases_never_become_json_output_even_after_rename(tmp_path):
    source=tmp_path/"input.bin";source.write_bytes(b"original");alias=tmp_path/"hardlink.bin";os.link(source,alias)
    value=q.fingerprint_file(str(source));job=q.FileFingerprintJob()
    with pytest.raises(ValueError):job.request(str(source),str(source))
    with pytest.raises(ValueError):q.save_file_fingerprint(value,str(alias))
    assert job.request(str(source),str(alias));assert job.wait().state=="failed" and alias.read_bytes()==b"original"
    moved=tmp_path/"moved.bin";source.rename(moved)
    with pytest.raises(ValueError):q.save_file_fingerprint(value,str(moved))
    output=tmp_path/"old-observation.json";q.save_file_fingerprint(value,str(output))
    assert json.loads(output.read_text(encoding="utf-8"))["path"]==str(source.resolve()) and moved.read_bytes()==b"original"


def test_atomic_json_failure_keeps_directory_and_observation_is_deterministic(tmp_path):
    source=tmp_path/"source.bin";source.write_bytes(b"abc");value=q.fingerprint_file(str(source));folder=tmp_path/"directory";folder.mkdir()
    with pytest.raises((ValueError,RuntimeError)):q.save_file_fingerprint(value,str(folder))
    assert folder.is_dir() and not list(tmp_path.glob("*.qpx-tmp-*"))
    output=tmp_path/'quoted-name.json';q.save_file_fingerprint(value,str(output));before=output.read_bytes();q.save_file_fingerprint(value,str(output))
    assert output.read_bytes()==before==q.file_fingerprint_json(value).encode()


def test_cancel_progress_and_late_cancel_preserve_publication_contract(tmp_path):
    source=tmp_path/"large.bin"
    with source.open("wb") as stream:stream.truncate(128*1024*1024)
    output=tmp_path/"cancel.json";output.write_bytes(b"sentinel");job=q.FileFingerprintJob()
    assert job.request(str(source),str(output));job.cancel();status=job.wait()
    assert status.state in {"cancelled","succeeded"}
    if status.state=="cancelled":assert job.result is None and output.read_bytes()==b"sentinel"
    else:assert job.result.bytes==source.stat().st_size and json.loads(output.read_text())["sha256"]==job.result.sha256
    assert not list(tmp_path.glob("*.qpx-tmp-*"))
    small=tmp_path/"small.bin";small.write_bytes(b"abc");assert job.request(str(small));assert job.wait().state=="succeeded"
    job.cancel();assert job.poll().state=="succeeded" and job.result.sha256==hashlib.sha256(b"abc").hexdigest()


def test_native_wait_releases_gil_and_progress_is_monotonic(tmp_path):
    source=tmp_path/"threaded.bin";source.write_bytes(bytes(range(256))*262144)
    job=q.FileFingerprintJob();assert job.request(str(source));seen=[]
    while job.busy:
        status=job.poll();seen.append(status.bytes);assert status.bytes<=status.total_bytes
        time.sleep(.001)
    status=job.wait();assert status.state=="succeeded" and status.bytes==source.stat().st_size and seen==sorted(seen)
    calls=[0];stop=threading.Event()
    def counter():
        while not stop.is_set():calls[0]+=1
    thread=threading.Thread(target=counter);thread.start()
    try:
        before=calls[0];q.fingerprint_file(str(source));assert calls[0]>before
        assert job.request(str(source));before=calls[0];assert job.wait().state=="succeeded" and calls[0]>before
    finally:stop.set();thread.join()


def test_regular_file_observation_handles_concurrent_writer_without_false_success(tmp_path):
    source=tmp_path/"busy.bin";source.write_bytes(b"abc")
    if os.name=="nt":
        with source.open("r+b"):
            job=q.FileFingerprintJob();assert job.request(str(source));assert job.wait().state=="failed" and job.result is None
    else:
        # POSIX does not enforce mandatory deny-write sharing. Force an observable
        # timestamp change once the native reader has started.
        with source.open("wb") as stream:stream.truncate(256*1024*1024)
        job=q.FileFingerprintJob();assert job.request(str(source));deadline=time.monotonic()+5
        while job.busy and job.poll().bytes==0 and time.monotonic()<deadline:time.sleep(.001)
        assert job.busy
        stamp=source.stat();os.utime(source,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+2_000_000_000))
        assert job.wait().state=="failed" and job.result is None


def test_worker_and_result_keep_no_python_source_owners_on_shutdown(tmp_path):
    script=r'''import gc,hashlib,pathlib,sys
import qupathxx as q
folder=pathlib.Path(sys.argv[1]);source=folder/"child.bin";source.write_bytes(b"abc"*100000)
job=q.FileFingerprintJob();assert job.request(str(source),str(folder/"child.json"));del source;gc.collect()
assert job.wait().state=="succeeded" and job.result.sha256==hashlib.sha256(b"abc"*100000).hexdigest()
value=job.result;del job;gc.collect();assert value.bytes==300000
'''
    child=subprocess.run([sys.executable,"-I","-c",script,str(tmp_path)],capture_output=True,text=True,timeout=20)
    assert child.returncode==0,child.stdout+child.stderr


@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI")!="1",reason="desktop replay")
@pytest.mark.parametrize("finite",[False,True])
def test_gui_file_hash_exports_json_without_selected_annotation(tmp_path,finite):
    source=tmp_path/"\u96ea.bin";source.write_bytes(b"source bytes");output=tmp_path/"gui.json";doc=q.Document(q.Image.demo())
    config=q.GuiOptions();config.file_fingerprint_path=str(source);config.file_fingerprint_output=str(output)
    start=q.GuiInput();start.frame,start.kind=4,q.GuiInputKind.FINGERPRINT_FILE_KEY;config.inputs=[start]
    if not finite:
        ready=q.GuiInput();ready.frame,ready.kind,ready.wait_for=6,q.GuiInputKind.MOVE,q.GuiInputWait.FILE_FINGERPRINT_READY;config.inputs=[start,ready]
    report=q.show(doc,frame_limit=10 if finite else 24,options=config)
    assert report.file_fingerprint_state=="succeeded" and not report.file_fingerprint_error and report.selected_ids==[]
    assert report.file_fingerprint_result.sha256==hashlib.sha256(b"source bytes").hexdigest()
    assert output.read_bytes()==q.file_fingerprint_json(report.file_fingerprint_result).encode() and report.file_fingerprint_bytes==len(b"source bytes")


@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI")!="1",reason="desktop replay")
def test_gui_failed_file_hash_preserves_old_output_and_document(tmp_path):
    source=tmp_path/"limited.bin";source.write_bytes(b"abc");output=tmp_path/"old.json";output.write_bytes(b"sentinel")
    doc=q.Document(q.Image.demo());obj=doc.add_rectangle(q.Rect(1,1,4,4));revision=doc.revision
    config=q.GuiOptions();config.file_fingerprint_path=str(source);config.file_fingerprint_output=str(output);config.file_fingerprint_byte_limit=2
    event=q.GuiInput();event.frame,event.kind=4,q.GuiInputKind.FINGERPRINT_FILE_KEY;config.inputs=[event]
    report=q.show(doc,frame_limit=20,options=config)
    assert report.file_fingerprint_state=="failed" and "byte limit" in report.file_fingerprint_error and report.file_fingerprint_result is None
    assert output.read_bytes()==b"sentinel" and doc.revision==revision and doc.annotation(obj).id==obj
