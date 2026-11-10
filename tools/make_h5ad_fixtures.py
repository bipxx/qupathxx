"""Development-only independent HDF5 fixtures. Product/CTest never import h5py."""
from pathlib import Path
import hashlib,json
import h5py
import numpy as np
root=Path(__file__).resolve().parents[1]/'tests/fixtures/h5ad'
root.mkdir(parents=True,exist_ok=True)
text=h5py.string_dtype('utf-8')
values=np.array([[0,7,0,9007199254740993],[3,0,5,0],[0,11,0,0]],dtype='<u8')
def encoding(obj,kind,version='0.2.0'):
    obj.attrs['encoding-type']=kind;obj.attrs['encoding-version']=version

def write(name,kind='array',continuous=False):
    path=root/(name+'.h5ad')
    with h5py.File(path,'w',track_order=True) as f:
        encoding(f,'anndata','0.1.0')
        for key,ids in [('obs',['spot_c','spot_a','spot_b']),('var',['gene_z','gene_a','gene_α','gene_b'])]:
            g=f.create_group(key);encoding(g,'dataframe');g.attrs['_index']='_index';g.attrs.create('column-order',np.array([],dtype=text));d=g.create_dataset('_index',data=np.array(ids,dtype=text));encoding(d,'string-array')
        a=values if not continuous else np.array([[0,-1.25,0,2.5],[.125,0,3.75,0],[0,11,0,0]],dtype='>f8')
        if kind=='array':
            d=f.create_dataset('X',data=a,chunks=a.shape,compression='gzip',shuffle=True,fletcher32=True);encoding(d,'array')
        else:
            g=f.create_group('X');encoding(g,kind,'0.1.0');g.attrs['shape']=np.array(a.shape,dtype='<i8');pointers=[0];indices=[];data=[]
            for row in (a if kind=='csr_matrix' else a.T):
                idx=np.flatnonzero(row)[::-1];indices.extend(idx);data.extend(row[idx]);pointers.append(len(indices))
            g.create_dataset('indptr',data=np.array(pointers,dtype='>i8'));g.create_dataset('indices',data=np.array(indices,dtype='<i4'));g.create_dataset('data',data=np.array(data,dtype=a.dtype),compression='gzip')
        g=f.create_group('obsm');encoding(g,'dict','0.1.0');d=g.create_dataset('spatial',data=np.array([[100.25,-.5],[8,9],[0,12.5]],dtype='>f8'));encoding(d,'array')
        f.create_group('uns').attrs['ignored']='not a round-trip promise'
    return path

def variant(name,mutator,kind='csr_matrix'):
    path=write(name,kind)
    with h5py.File(path,'r+') as f:mutator(f)

def replace(f,path,data):
    attrs=dict(f[path].attrs);del f[path];d=f.create_dataset(path,data=data)
    for k,v in attrs.items():d.attrs[k]=v

for kind in ['array','csr_matrix','csc_matrix']:
    write(kind,kind);write(kind+'-continuous',kind,True)
variant('duplicate-observation',lambda f:f['obs/_index'].__setitem__(1,'spot_c'))
variant('duplicate-feature',lambda f:f['var/_index'].__setitem__(1,'gene_z'))
variant('long-id',lambda f:f['obs/_index'].__setitem__(1,'x'*300))
variant('matrix-shape',lambda f:f['X'].attrs.__setitem__('shape',[4,3]))
variant('spatial-shape',lambda f:replace(f,'obsm/spatial',np.zeros((3,3))))
variant('pointer-negative',lambda f:f['X/indptr'].__setitem__(1,-1))
variant('pointer-order',lambda f:f['X/indptr'].__setitem__(2,1))
variant('pointer-end',lambda f:f['X/indptr'].__setitem__(-1,4))
variant('index-negative',lambda f:f['X/indices'].__setitem__(0,-1))
variant('index-range',lambda f:f['X/indices'].__setitem__(0,4))
variant('index-float',lambda f:replace(f,'X/indices',np.array([3,1,2,0,1],dtype='f8')))
variant('duplicate-coordinate',lambda f:f['X/indices'].__setitem__(0,1))
variant('count-negative',lambda f:replace(f,'X/data',np.array([-1,7,5,3,11],dtype='i8')))
variant('count-fractional',lambda f:replace(f,'X/data',np.array([1.5,7,5,3,11],dtype='f8')))
variant('nonfinite',lambda f:replace(f,'X/data',np.array([float('nan'),7,5,3,11],dtype='f8')))
variant('future-version',lambda f:f.attrs.__setitem__('encoding-version','9.9.9'))
variant('long-attribute',lambda f:f.attrs.__setitem__('encoding-version','0.1.0'+'x'*300))
variant('soft-link',lambda f:(f.move('X','old_X'),f.__setitem__('X',h5py.SoftLink('/old_X'))))
variant('external-link',lambda f:(f.__delitem__('X'),f.__setitem__('X',h5py.ExternalLink('never-open-this.h5ad','/X'))))
variant('external-storage',lambda f:(f.__delitem__('X'),encoding(f.create_dataset('X',(3,4),dtype='u8',external=[('never-read-this.bin',0,h5py.h5f.UNLIMITED)]),'array')))
def virtual(f):
    del f['X'];layout=h5py.VirtualLayout((3,4),'u8');layout[:]=h5py.VirtualSource('never-open-this.h5ad','/X',(3,4));encoding(f.create_virtual_dataset('X',layout),'array')
variant('virtual',virtual)
variant('unreviewed-filter',lambda f:(f.__delitem__('X'),encoding(f.create_dataset('X',data=values,compression='lzf'),'array')))
path=write('selected-layer')
with h5py.File(path,'r+') as f:
    encoding(f.create_group('layers'),'dict','0.1.0');f.move('X','layers/counts');f.create_group('X').attrs['encoding-type']='not-selected'
manifest={'generator':'tools/make_h5ad_fixtures.py','generator_h5py':h5py.__version__,'purpose':'Independent authored format/axis/domain and malformed storage fixtures; no third-party biological input','license':'AGPL-3.0-only','files':[]}
for p in sorted(root.glob('*.h5ad')):manifest['files'].append({'filename':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(len(manifest['files']),'fixtures',sum(x['bytes'] for x in manifest['files']),'bytes')
