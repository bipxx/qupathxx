"""Independent classic-TIFF writer with Pillow JPEG streams/decoded expectations.

No qupathxx, native libtiff, imagecodecs, or tifffile encoder is used. Expected
samples are decoded JPEG values, never the lossy encoder's original input.
"""
import io
import struct

import numpy as np
from PIL import Image


def split_tables(stream):
    tables=[];image=[];offset=2
    assert stream[:2]==b'\xff\xd8'
    while stream[offset:offset+2]!=b'\xff\xda':
        marker=stream[offset+1];length=int.from_bytes(stream[offset+2:offset+4],'big')
        segment=stream[offset:offset+length+2]
        (tables if marker in (0xdb,0xc4,0xcc) else image).append(segment)
        offset+=length+2
    return b'\xff\xd8'+b''.join(tables)+b'\xff\xd9',b'\xff\xd8'+b''.join(image)+stream[offset:]


def generate(path, *, rgb=False, tiled=False, tables=False, byteorder='<', white=False):
    width,height=39,35;channels=3 if rgb else 1
    y,x=np.indices((height,width));values=((x*11+y*7)%256).astype('uint8')
    if rgb:values=np.stack((values,(x*3+y*13)%256,(x*19+y*5)%256),axis=-1).astype('uint8')
    expected=np.zeros_like(values);streams=[];shared=None
    bw,bh=(16,16) if tiled else (width,16)
    for top in range(0,height,bh):
        for left in range(0,width,bw):
            h,w=min(bh,height-top),min(bw,width-left)
            shape=(bh,bw,3) if rgb else (bh,bw)
            block=np.zeros(shape,dtype='uint8');block[:h,:w]=values[top:top+h,left:left+w]
            if not tiled:block=block[:h]
            output=io.BytesIO();Image.fromarray(block).save(output,format='JPEG',quality=91,subsampling=0,keep_rgb=rgb)
            stream=output.getvalue()
            with Image.open(io.BytesIO(stream)) as decoded:expected[top:top+h,left:left+w]=np.asarray(decoded)[:h,:w]
            if tables:
                table,stream=split_tables(stream)
                if shared is not None:assert table==shared
                shared=table
            streams.append(stream)
    # Classic TIFF IFD and primitive arrays, all offsets measured from its header.
    # Types: SHORT=3, LONG=4, ASCII=2, UNDEFINED=7.
    description=(f'<OME><Image ID="Image:0"><Pixels ID="Pixels:0" DimensionOrder="XYZCT" '
                 f'Type="uint8" SizeX="{width}" SizeY="{height}" SizeC="{channels}" SizeZ="1" SizeT="1" '
                 'PhysicalSizeX=".25" PhysicalSizeY=".5"><Channel ID="Channel:0:0" '
                 f'SamplesPerPixel="{channels}"/><TiffData IFD="0" PlaneCount="1"/></Pixels></Image></OME>\0').encode()
    tags={256:(4,[width]),257:(4,[height]),258:(3,[8]*channels),259:(3,[7]),
          262:(3,[2 if rgb else int(not white)]),270:(2,description),274:(3,[1]),277:(3,[channels]),284:(3,[1])}
    offset_tag,count_tag=(324,325) if tiled else (273,279)
    tags[count_tag]=(4,[len(s) for s in streams]);tags[offset_tag]=(4,[0]*len(streams))
    if tiled:tags.update({322:(4,[bw]),323:(4,[bh])})
    else:tags[278]=(4,[bh])
    if tables:tags[347]=(7,shared)
    start=8+2+12*len(tags)+4;extra=bytearray();entries=[]
    def encode(kind,items):
        return bytes(items) if kind in (2,7) else struct.pack(byteorder+('H' if kind==3 else 'I')*len(items),*items)
    # Reserve variable-length fields first, including the offsets array.
    positions={}
    for tag,(kind,items) in sorted(tags.items()):
        data=encode(kind,items)
        if len(data)>4:
            positions[tag]=start+len(extra);extra.extend(data)
            if len(extra)%2:extra.append(0)
    data_start=start+len(extra);offsets=[]
    for stream in streams:offsets.append(data_start);data_start+=len(stream)
    tags[offset_tag]=(4,offsets)
    for tag,(kind,items) in sorted(tags.items()):
        data=encode(kind,items)
        if len(data)>4:
            position=positions[tag];extra[position-start:position-start+len(data)]=data
            value=struct.pack(byteorder+'I',position)
        else:value=data.ljust(4,b'\0')
        entries.append(struct.pack(byteorder+'HHI',tag,kind,len(items))+value)
    header=(b'II' if byteorder=='<' else b'MM')+struct.pack(byteorder+'HI',42,8)
    path.write_bytes(header+struct.pack(byteorder+'H',len(tags))+b''.join(entries)+b'\0'*4+extra+b''.join(streams))
    return expected,offsets
