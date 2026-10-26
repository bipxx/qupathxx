"""Bounded diagnostics controls; actual LLDB/ABI probe also runs on arm64."""
from pathlib import Path
import importlib.util
import json
import os
import stat
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile
ROOT=Path(__file__).resolve().parents[1]

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);exec(compile(path.read_bytes(),str(path),'exec',dont_inherit=True),value.__dict__);return value
R=module('retained_replay',ROOT/'replay_retained_arm64.py')

class ArchiveTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def archive(self,names):
        path=self.root/'input.zip'
        with zipfile.ZipFile(path,'w') as z:
            for name,data in names:z.writestr(name.replace(chr(92),'/'),data)
        raw=path.read_bytes()
        for name,_ in names:
            if chr(92) in name:
                normal=name.replace(chr(92),'/').encode();require_count=raw.count(normal)
                self.assertEqual(require_count,2);raw=raw.replace(normal,name.encode())
        path.write_bytes(raw)
        return path
    def test_valid_regular_archive(self):
        archive=self.archive([('sdk/file',b'actual')]);R.safe_extract(archive,self.root/'out');self.assertEqual((self.root/'out/sdk/file').read_bytes(),b'actual')
    def test_bad_paths(self):
        for index,name in enumerate(['../escape','/absolute','C:/drive','sdk\\escape','..\\escape']):
            with self.subTest(name=name):
                archive=self.archive([(name,b'x')]);out=self.root/('out-'+str(index))
                with self.assertRaises(RuntimeError):R.safe_extract(archive,out)
                self.assertFalse(out.exists())
    def test_case_collision(self):
        archive=self.archive([('SDK/file',b'a'),('sdk/FILE',b'b')])
        with self.assertRaises(RuntimeError):R.safe_extract(archive,self.root/'out')
    def test_special_member(self):
        path=self.root/'input.zip';item=zipfile.ZipInfo('link');item.create_system=3;item.external_attr=(stat.S_IFLNK|0o777)<<16
        with zipfile.ZipFile(path,'w') as z:z.writestr(item,'outside')
        with self.assertRaises(RuntimeError):R.safe_extract(path,self.root/'out')
    def test_admission_before_creation(self):
        archive=self.archive([('large',b'1234')])
        with self.assertRaises(RuntimeError):R.safe_extract(archive,self.root/'out',max_bytes=3)
        self.assertFalse((self.root/'out').exists())
    def test_existing_destination_preserved(self):
        archive=self.archive([('data',b'new')]);out=self.root/'out';out.mkdir();(out/'sentinel').write_bytes(b'prior')
        with self.assertRaises(RuntimeError):R.safe_extract(archive,out)
        self.assertEqual((out/'sentinel').read_bytes(),b'prior')
    def test_wrong_original_archive(self):
        archive=self.archive([('untrusted',b'wrong')])
        with self.assertRaisesRegex(RuntimeError,'digest mismatch'):R.prepare(archive,self.root)
    def test_ambiguous_product(self):
        for folder in ['a','b']:(self.root/folder).mkdir();(self.root/folder/'product').write_bytes(b'x')
        with self.assertRaises(RuntimeError):R.one(self.root,'product')
    def test_diagnostic_commands_do_not_change_inferior(self):
        text=R.debug_commands('/target/program','/observer.py','/runtime/lib');self.assertIn('target.disable-aslr false',text);self.assertIn('DYLD_LIBRARY_PATH',text)
        self.assertNotIn('expression',text);self.assertNotIn('register write',text);self.assertNotIn('thread return',text)

class Error:
    def __init__(self):self.ok=True
    def Success(self):return self.ok
class Value:
    def __init__(self,value,valid=True):self.value=value;self.valid=valid
    def IsValid(self):return self.valid
    def GetValueAsUnsigned(self):return self.value
class Target:
    def GetTriple(self):return 'arm64-apple-macos'
    def GetAddressByteSize(self):return 8
class Process:
    def __init__(self):self.pointers={0x2008:0x3000,0x1008:0x4000};self.strings={0x3000:'St16invalid_argument',0x4000:'raw grid source series changed'};self.reads=[]
    def GetTarget(self):return Target()
    def ReadPointerFromMemory(self,address,error):
        self.reads.append(address)
        if address not in self.pointers:error.ok=False;return 0
        return self.pointers[address]
    def ReadCStringFromMemory(self,address,count,error):
        if address not in self.strings:error.ok=False;return ''
        return self.strings[address]
class Thread:
    def __init__(self,process):self.process=process
    def GetProcess(self):return self.process
    def GetThreadID(self):return 7
class Frame:
    def __init__(self,process):self.thread=Thread(process);self.registers={'x0':Value(0x1000),'x1':Value(0x2000)}
    def GetThread(self):return self.thread
    def FindRegister(self,name):return self.registers.get(name,Value(0,False))

class CallbackTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.environment=patch.dict(os.environ,{'QPX_EXCEPTION_LOG':str(self.root/'events.jsonl')});self.environment.start()
        with patch.dict(sys.modules,{'lldb':types.SimpleNamespace(SBError=Error,LLDB_INVALID_ADDRESS=2**64-1)}):self.observer=module('retained_observer',ROOT/'lldb_retained_exceptions.py')
        self.observer.stack=lambda frame:[{'function':'control'}];self.process=Process();self.frame=Frame(self.process)
    def tearDown(self):self.environment.stop();self.tmp.cleanup()
    def test_throw_message_and_type(self):
        self.assertFalse(self.observer.on_throw(self.frame,None,{}));event=self.observer.EVENTS[0];self.assertEqual(event['type'],'St16invalid_argument');self.assertEqual(event['message'],'raw grid source series changed');self.assertFalse(self.observer.ERRORS)
    def test_catch_matches_exact_object(self):
        self.observer.on_throw(self.frame,None,{});self.frame.registers['x0']=Value(0x1000-32);self.assertFalse(self.observer.on_catch(self.frame,None,{}));self.assertEqual(self.observer.EVENTS[-1]['matched_throw'],1)
    def test_wrong_type_is_distinguished(self):
        self.process.strings[0x3000]='St13runtime_error';self.observer.on_throw(self.frame,None,{});self.assertEqual(self.observer.EVENTS[0]['type'],'St13runtime_error')
    def test_unknown_type_does_not_read_message_layout(self):
        self.process.strings[0x3000]='i';self.observer.on_throw(self.frame,None,{});self.assertNotIn(0x1008,self.process.reads);self.assertIsNone(self.observer.EVENTS[0]['message'])
    def test_read_error_is_explicit(self):
        self.process.pointers.clear();self.assertFalse(self.observer.on_throw(self.frame,None,{}));self.assertEqual(self.observer.EVENTS[0]['event'],'observer_error');self.assertTrue(self.observer.ERRORS)
    def test_unmatched_catch_is_explicit(self):
        self.observer.on_catch(self.frame,None,{});self.assertIsNone(self.observer.EVENTS[0]['matched_throw'])
    def test_event_limit_rejects(self):
        self.observer.EVENTS=[{}]*20000
        with self.assertRaises(RuntimeError):self.observer.emit({'event':'extra'})

class ShallowCheckoutTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('git'), 'git required for actual shallow-checkout control')
    def test_exact_fetch_admits_original_but_rejects_changed_product(self):
        workflow=(ROOT.parents[1]/'.github/workflows/ci.yml').read_text(encoding='utf-8')
        self.assertIn('run: git fetch --no-tags --depth=1 origin '+R.COMMIT,workflow)
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'source';source.mkdir()
            def git(cwd,*args,check=True):
                return subprocess.run(['git','-C',str(cwd),*args],check=check,capture_output=True,text=True,timeout=30)
            git(source,'init','-q');(source/'cpp').mkdir();(source/'cpp/product.cpp').write_text('original\n')
            def commit():
                git(source,'add','.')
                git(source,'-c','user.name=Diagnostic','-c','user.email=diagnostic@example.invalid','-c','commit.gpgsign=false','commit','-qm','fixture')
            commit();original=git(source,'rev-parse','HEAD').stdout.strip()
            (source/'observer.txt').write_text('observer only\n');commit()
            checkout=Path(directory)/'checkout'
            git(source,'clone','--no-local','--depth=1',str(source),str(checkout))
            self.assertEqual(git(checkout,'diff','--exit-code',original,'--','cpp',check=False).returncode,128)
            git(checkout,'fetch','--no-tags','--depth=1','origin',original)
            git(checkout,'diff','--exit-code',original,'--','cpp')
            (checkout/'cpp/product.cpp').write_text('changed\n')
            self.assertEqual(git(checkout,'diff','--exit-code',original,'--','cpp',check=False).returncode,1)

if __name__=='__main__':unittest.main()
