"""Pack a synthetic QCL5 vector independently with Python struct/hashlib.

This describes a historical numeric snapshot, not a segmentation oracle. Its
input digest is an explicit synthetic placeholder; it grants no publication
authority and is not evidence of pixels captured from a real reader.
"""
import argparse
from decimal import Decimal, localcontext
import hashlib
import json
from pathlib import Path
import struct


def integer(value):return struct.pack('<Q',value)
def real(value):return struct.pack('<d',value)
def blob(value):return integer(len(value))+value
def text(value):return blob(value.encode('utf-8'))
def optional(value):return integer(value is not None)+(real(value) if value is not None else b'')


def reference():
    provenance=b'QPV\2'+b''.join(integer(v) for v in (1,1,1,0,0,1,1))
    provenance+=text('oracle.rgba8/1')+text('')+text('synthetic')
    provenance+=b''.join(optional(v) for v in (.5,.25,.5,.25))
    provenance+=integer(1)+integer(1)+integer(1)+real(1)
    environment=('0.1.0','0'*64,'oracle','independent Python struct','','reference','portable','none','','','')
    provenance+=b''.join(text(v) for v in environment)+integer(0)*4
    provenance+=hashlib.sha256(provenance).hexdigest().encode('ascii')
    packet=b'QCL\5'+b''.join(integer(v) for v in (4,1,0,0))
    packet+=b''.join(real(v) for v in (1,0,.1,0,0,0,.125))
    packet+=integer(0)+b''.join(integer(v) for v in (10000,256*1024*1024,1,10000,250000,250000,1024*1024))
    packet+=b''.join(integer(v) for v in (1,1,0,0,1,1,1,1,0,0))+blob(provenance)
    packet+=b''.join(real(v) for v in (255,255,255,1,0,0,0,1,0,0,0,1))
    packet+=b''.join(real(v) for v in (.5,.25,0,0))+integer(1)*2
    packet+=integer(1)*5+real(0)+b'a'*64
    packet+=struct.pack('<IIId',1,1,0,.25)
    with localcontext() as context:
        context.prec=70;od=float((Decimal(255)/Decimal(32)).log10())
    packet+=integer(1)*2+b''.join(real(v) for v in (32,32,32,32,32,32,od,od,od,od,od,od))
    # Each one-sample label array uses raw encoding; RLE would be larger.
    packet+=(integer(0)+integer(1)+struct.pack('<I',1))*2+blob(b'\1')
    geometry=b'QRO\1\0'+struct.pack('<dddd',0,0,1,1)
    packet+=hashlib.sha256(blob(geometry)+packet).hexdigest().encode('ascii')
    metadata=dict(schema='qpx.qcl5-independent-vector/1',scope='Synthetic historical numeric snapshot; no sampled input or segmentation acceptance',
                  bytes=len(packet),sha256=hashlib.sha256(packet).hexdigest(),roi=[0,0,1,1],image=[1,1],units='micrometres',
                  sample_steps=[.5,.25],nucleus_area_um2=.125,nucleus_rgb=[32]*3,nucleus_channels=[od]*3,
                  input_sha256='a'*64,provenance_sha256=hashlib.sha256(provenance).hexdigest())
    return packet,metadata


def native_header(packet):
    rows=[','.join(str(v) for v in packet[at:at+32]) for at in range(0,len(packet),32)]
    return '#pragma once\nnamespace grid_cell_archive_reference {\ninline constexpr unsigned char bytes[]={\n'+',\n'.join(rows)+'\n};\n}\n'


if __name__=='__main__':
    p=argparse.ArgumentParser()
    for key in ('output','metadata','native-header'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    if any(path.exists() for path in (a.output,a.metadata,a.native_header)):p.error('reference outputs must not exist')
    packet,metadata=reference();a.output.write_bytes(packet)
    a.metadata.write_bytes((json.dumps(metadata,indent=2)+'\n').encode())
    a.native_header.write_bytes(native_header(packet).encode())
