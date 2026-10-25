"""Generate an independent two-file OME C/Z/T and SubIFD reader corpus.

All sample values derive from axis coordinates, never qupathxx observations.
The files have shuffled plane placement and different byte orders/codecs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
import tifffile

NAMESPACE='http://www.openmicroscopy.org/Schemas/OME/2016-06'
FILES=('primary.ome.tif','companions/planes-\u03b2.ome.tif')
UUIDS=('urn:uuid:00000000-0000-4000-8000-000000000001',
       'urn:uuid:00000000-0000-4000-8000-000000000002')
# (T,Z,C) in physical IFD order. Explicit FirstC/Z/T maps all eight planes.
PLANES=(((1,0,1),(0,1,0),(0,0,0),(1,1,1)),
        ((0,0,1),(1,1,0),(1,0,0),(0,1,1)))


def values(t,z,c,scale=1):
    y=np.arange(0,17,scale,dtype=np.uint16)[:,None]
    x=np.arange(0,19,scale,dtype=np.uint16)[None,:]
    return np.asarray(10000*t+1000*z+100*c+19*y+x,dtype=np.uint16)


def metadata(file_index):
    ET.register_namespace('',NAMESPACE)
    def tag(name): return '{'+NAMESPACE+'}'+name
    root=ET.Element(tag('OME'),UUID=UUIDS[file_index])
    image=ET.SubElement(root,tag('Image'),ID='Image:0',Name='Independent synthetic multi-file axes')
    pixels=ET.SubElement(image,tag('Pixels'),ID='Pixels:0',DimensionOrder='XYZCT',
        Type='uint16',SizeX='19',SizeY='17',SizeC='2',SizeZ='2',SizeT='2',
        PhysicalSizeX='0.25',PhysicalSizeY='0.5',PhysicalSizeXUnit='\u00b5m',PhysicalSizeYUnit='\u00b5m')
    for c in range(2):
        ET.SubElement(pixels,tag('Channel'),ID=f'Channel:0:{c}',Name=f'Raw {c}',SamplesPerPixel='1')
    for other,planes in enumerate(PLANES):
        # Reference names are relative to the file containing this XML.
        filename=FILES[other] if file_index==0 else ('../'+FILES[0] if other==0 else Path(FILES[1]).name)
        for ifd,(t,z,c) in enumerate(planes):
            entry=ET.SubElement(pixels,tag('TiffData'),IFD=str(ifd),FirstT=str(t),FirstZ=str(z),FirstC=str(c),PlaneCount='1')
            ET.SubElement(entry,tag('UUID'),FileName=filename).text=UUIDS[other]
    return ET.tostring(root,encoding='ascii',xml_declaration=True).decode('ascii')


def generate(directory):
    directory.mkdir(parents=True,exist_ok=False)
    manifest=dict(schema='qpx.multifile-ome-fixture/1',synthetic=True,patient_data=False,
        specification='https://ome-model.readthedocs.io/en/stable/ome-tiff/specification.html#multi-file-ome-tiff',
        scope='Independent reader fixture; native multi-file support is not established by fixture generation.',
        sample_equation='uint16(10000*T + 1000*Z + 100*C + 19*level_zero_y + level_zero_x)',
        dimension_order='XYZCT',shape_tczyx=[2,2,2,17,19],downsamples=[1,2],
        calibration_um=[.25,.5],files=[])
    for index,name in enumerate(FILES):
        path=directory/name;path.parent.mkdir(parents=True,exist_ok=True)
        planes=[]
        with tifffile.TiffWriter(path,byteorder='<' if index==0 else '>',ome=False) as writer:
            for ifd,(t,z,c) in enumerate(PLANES[index]):
                levels=[]
                for level,scale in enumerate((1,2)):
                    pixels=values(t,z,c,scale)
                    writer.write(pixels,photometric='minisblack',tile=(16,16),
                        compression='deflate' if index==0 else None,metadata=None,
                        description=metadata(index) if ifd==level==0 else None,
                        subifds=1 if level==0 else None,subfiletype=level,maxworkers=1)
                    levels.append(dict(level=level,width=pixels.shape[1],height=pixels.shape[0],
                        canonical_le_sha256=hashlib.sha256(pixels.astype('<u2').tobytes()).hexdigest()))
                planes.append(dict(ifd=ifd,t=t,z=z,c=c,levels=levels))
        raw=path.read_bytes()
        manifest['files'].append(dict(name=name,uuid=UUIDS[index],bytes=len(raw),
            sha256=hashlib.sha256(raw).hexdigest(),byteorder='little' if index==0 else 'big',planes=planes))
    (directory/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode('utf-8'))
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    args=parser.parse_args();manifest=generate(args.output)
    print(json.dumps(dict(files=len(manifest['files']),planes=sum(len(f['planes']) for f in manifest['files']),
        output=str(args.output.resolve())),indent=2))


if __name__=='__main__': main()
