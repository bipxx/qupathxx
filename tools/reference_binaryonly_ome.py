"""Independent BinaryOnly filesets derived from the scalar multi-file corpus."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import posixpath
import xml.etree.ElementTree as ET

import tifffile

spec=importlib.util.spec_from_file_location('binaryonly_planes',Path(__file__).with_name('reference_multifile_ome.py'))
planes=importlib.util.module_from_spec(spec);spec.loader.exec_module(planes)
METADATA_UUID='urn:uuid:7e59401c-37c7-481a-a28e-5932cc4b0760'
METADATA_FILE='metadata/元数据.companion.ome'


def tag(name):return '{'+planes.NAMESPACE+'}'+name


def generate(directory,master='xml'):
    if master not in ('xml','tiff','tiff-default','tiff-be'):raise ValueError('unknown metadata master')
    directory=Path(directory);planes.generate(directory)
    root=ET.fromstring(planes.metadata(0))
    master_index=1 if master=='tiff-be' else 0
    metadata_name=METADATA_FILE if master=='xml' else planes.FILES[master_index]
    metadata_uuid=METADATA_UUID if master=='xml' else planes.UUIDS[master_index]
    root.set('UUID',metadata_uuid)
    for reference in root.iter(tag('UUID')):
        filename=reference.get('FileName')
        reference.set('FileName',posixpath.relpath(filename,posixpath.dirname(metadata_name) or '.'))
    if master=='tiff-default':
        for entry in root.iter(tag('TiffData')):
            reference=entry.find(tag('UUID'))
            if reference.text==planes.UUIDS[0]:entry.remove(reference)
    full=ET.tostring(root,encoding='ascii',xml_declaration=True).decode('ascii')
    if master=='xml':
        path=directory/metadata_name;path.parent.mkdir();path.write_bytes(full.encode('ascii'))
    for index,name in enumerate(planes.FILES):
        if master!='xml' and index==master_index:
            text=full
        else:
            partial=ET.Element(tag('OME'),UUID=planes.UUIDS[index])
            ET.SubElement(partial,tag('BinaryOnly'),UUID=metadata_uuid,
                          MetadataFile=posixpath.relpath(metadata_name,posixpath.dirname(name) or '.'))
            text=ET.tostring(partial,encoding='ascii',xml_declaration=True).decode('ascii')
        tifffile.tiffcomment(directory/name,text)
    names=[*planes.FILES]+([METADATA_FILE] if master=='xml' else [])
    result=dict(schema='qpx.binaryonly-ome-reference/1',synthetic=True,patient_data=False,
                specification='https://docs.openmicroscopy.org/ome-model/6.2.2/ome-tiff/specification.html#partial-ome-xml-metadata',
                master=master,metadata_file=metadata_name,metadata_uuid=metadata_uuid,
                sample_equation='uint16(10000*T + 1000*Z + 100*C + 19*level_zero_y + level_zero_x)',
                files=[dict(path=n,bytes=(directory/n).stat().st_size,sha256=hashlib.sha256((directory/n).read_bytes()).hexdigest()) for n in names])
    (directory/'manifest.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path)
    parser.add_argument('--master',choices=['xml','tiff','tiff-default','tiff-be'],default='xml')
    args=parser.parse_args();print(json.dumps(generate(args.directory,args.master),indent=2))
