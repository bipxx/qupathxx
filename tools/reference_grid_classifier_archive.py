"""Independent synthetic QCR1/QCA5 prediction snapshot, using struct/hashlib.

The one-node-scale forest is hand-authored protocol data, not trained research
evidence. Typed source metadata reuses the independently packed QCL5 descriptor.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qcl5_vector',ROOT/'tools/reference_grid_cell_archive.py')
qcl=importlib.util.module_from_spec(spec);spec.loader.exec_module(qcl)
u=qcl.integer;r=qcl.real;blob=qcl.blob;text=qcl.text
def small(v):return struct.pack('<I',v)
def short_text(v):return small(len(v.encode()))+v.encode()
def signed(payload):return payload+hashlib.sha256(payload).hexdigest().encode()


def reference():
    source,source_meta=qcl.reference();size=struct.unpack_from('<Q',source,236)[0];provenance=source[244:244+size]
    anchor=bytearray(provenance[:-64]);struct.pack_into('<QQQ',anchor,4,2,3,3);anchor=signed(anchor)
    schema=[('object.area','px2'),('object.perimeter','px'),('object.width','px'),('object.height','px'),('object.circularity','1')]
    schema +=[(comp+'.mean_'+channel,'device-rgb8') for comp in ('cell','nucleus') for channel in ('red','green','blue')]
    model=b''.join(small(v) for v in (0x46585051,1,2,2,2,0))+u(1)+u(4*1024*1024)+u(4)
    model+=short_text('a'*64)+short_text('independent-object-test/1')+small(len(schema))
    model+=b''.join(short_text(name)+short_text(unit) for name,unit in schema)
    model+=small(2)+short_text('A')+short_text('B')+b''.join(small(v) for v in (2,0,3,4))
    model+=b''.join(small(v) for row in ((0,1,2,0,0x3f800000),(0,0,0,0,0),(0,0,0,1,0),(0,0,0,1,0)) for v in row)
    geometry=b'QRO\1\0'+struct.pack('<dddd',0,0,1,1)
    values=[1,4,1,1,math.pi/4]+[32]*6
    bits=[struct.unpack('<I',struct.pack('<f',v))[0] for v in values]
    table=b'qpx.object-features/1'+u(1)+u(len(schema))+b''.join(text(name)+text(unit) for name,unit in schema)
    table+=u(2)+u(3)+u(2)+text('')+b''.join(u(v) for v in bits)
    table_sha=hashlib.sha256(table).hexdigest()
    artifact=b'QCA\5'+u(1)+blob(model)+blob(anchor)
    artifact+=u(4)+u(64*1024*1024)+u(0)+r(1)+r(1)+u(3)+u(3)+u(1)
    artifact+=u(2)+u(3)+u(2)+text('')+blob(geometry)+u(1)+blob(geometry)
    od=source_meta['nucleus_channels'][0]
    artifact+=u(1)+u(1)+b''.join(r(v) for v in [32]*6+[od]*6)+u(0)+u(2)+u(1)
    artifact+=u(1)+u(1)  # one distinct origin, tagged grid kind
    artifact+=u(2)+u(1)+u(1)+u(1)+u(1)+text('qupathxx/0.1.0')+text('qpx.grid-log-watershed-bodies/1')+text('a'*64)+blob(geometry)
    artifact+=source[4:236]+blob(provenance)+source[244+size:244+size+96+48]
    artifact+=text(table_sha)+u(len(values))+b''.join(u(v) for v in bits)+r(0)
    artifact=signed(artifact)
    record=signed(b'QCR\1'+u(4)+text('Synthetic grid classifier')+u(0)+r(0)+u(0)+blob(artifact))
    metadata=dict(schema='qpx.qca5-independent-vector/1',scope='Synthetic historical prediction protocol; hand-authored forest and placeholder input hash, not research/model acceptance',
        bytes=len(record),sha256=hashlib.sha256(record).hexdigest(),artifact_sha256=hashlib.sha256(artifact).hexdigest(),table_sha256=table_sha,
        source_qcl5_sha256=source_meta['sha256'],source_units='micrometres',source_steps=[.5,.25],rows=1,features=len(values))
    return record,metadata


def native_header(packet):
    rows=[','.join(str(v) for v in packet[at:at+32]) for at in range(0,len(packet),32)]
    return '#pragma once\nnamespace grid_classifier_archive_reference {\ninline constexpr unsigned char bytes[]={\n'+',\n'.join(rows)+'\n};\n}\n'


if __name__=='__main__':
    p=argparse.ArgumentParser()
    for key in ('output','metadata','native-header'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    if any(path.exists() for path in (a.output,a.metadata,a.native_header)):p.error('reference outputs must not exist')
    packet,metadata=reference();a.output.write_bytes(packet);a.metadata.write_bytes((json.dumps(metadata,indent=2)+'\n').encode());a.native_header.write_bytes(native_header(packet).encode())
