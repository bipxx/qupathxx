"""Plan explicit loader-relative Mac native relocation from a validated snapshot.

No binaries are extracted, modified, loaded or signed. Original SDK resolution
is not simulated; captured identities/explicit roots select package providers.
"""
import argparse
import importlib.util
import json
from pathlib import Path, PurePosixPath
import posixpath
import re

spec=importlib.util.spec_from_file_location('qpx_mac_plan_inventory',Path(__file__).with_name('macos_native_inventory.py'))
inventory=importlib.util.module_from_spec(spec);spec.loader.exec_module(inventory)
require=inventory.require
SYSTEM=frozenset([
    '/usr/lib/libSystem.B.dylib','/usr/lib/libc++.1.dylib','/usr/lib/libobjc.A.dylib',
    '/usr/lib/libiconv.2.dylib','/usr/lib/libresolv.9.dylib','/usr/lib/libz.1.dylib',
    '/usr/lib/libcompression.dylib',
    '/System/Library/Frameworks/Accelerate.framework/Versions/A/Accelerate',
    '/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation',
    '/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation',
    '/System/Library/Frameworks/Security.framework/Versions/A/Security',
])
SYSTEM_LEAVES=frozenset(PurePosixPath(value).name for value in SYSTEM)


def roots_policy(roots):
    require(isinstance(roots,dict) and set(roots)=={'native','geos','opencv'},'require native and both SDK source roots')
    result={}
    for key,value in roots.items():
        require(isinstance(value,str) and value.startswith('/') and len(value)>1
                and len(value.encode('utf-8'))<=4096 and posixpath.normpath(value)==value
                and not any(ord(c)<32 or ord(c)==127 or c=='\\' for c in value),
                'source roots must be canonical absolute POSIX paths')
        result[key]=value
    paths=list(result.values())
    require(all(a!=b and not a.startswith(b+'/') and not b.startswith(a+'/')
                for i,a in enumerate(paths) for b in paths[i+1:]),'overlapping Mac source roots')
    return result


def source_location(name, roots):
    parts=PurePosixPath(name).parts
    if parts[0]=='native':return roots['native']+'/'+parts[1]
    return roots[parts[1]]+'/'+PurePosixPath(*parts[2:]).as_posix()


def library_leaf(identity):
    leaf=PurePosixPath(identity).name
    require(re.fullmatch(r'[A-Za-z0-9_+.-]+\.dylib',leaf) is not None and leaf not in ['.dylib','..dylib'],
            'unsupported Mac package dylib filename')
    require(identity not in SYSTEM and leaf not in SYSTEM_LEAVES,
            'captured dylib identity shadows system runtime')
    return leaf


def create_plan(archive, architecture, source_roots):
    roots=roots_policy(source_roots)
    manifest=inventory.inspect_archive(archive,architecture)
    files={item['path']:item for item in manifest['files']}
    binaries={item['path']:item['slices'][0] for item in manifest['binaries']}
    groups={}
    for name,row in binaries.items():
        require(row['flags']&0x80 and not row['flags']&0x100,'flat/non-two-level namespace requires separate package policy')
        require(not any(row[key] for key in ['dyld_environment','sub_frameworks','sub_umbrellas','sub_clients','sub_libraries','interpreter_ids']),
                'Mach-O environment/subimage loading requires separate package policy')
        require(not row['interpreters'] or (name.startswith('native/') and row['interpreters']==['/usr/lib/dyld']),
                'unsupported Mac package interpreter')
        if name.startswith('native/'):continue
        require(row['dylib_id'] is not None,'captured dylib lacks install identity')
        leaf=library_leaf(row['dylib_id']['name'])
        digest=files[name]['sha256']
        group=groups.setdefault(digest,dict(aliases=[],row=row,target='lib/'+leaf))
        require(group['target']=='lib/'+leaf,'identical dylib bytes have inconsistent identity')
        group['aliases'].append(name)
    require(len({group['target'].casefold() for group in groups.values()})==len(groups),'dylib target name collision')
    for group in groups.values():group['aliases'].sort()
    by_source,by_identity,by_rpath={},{},{}
    def insert(mapping,key,group):
        require(key not in mapping or mapping[key] is group,'ambiguous captured dylib provider: '+key)
        mapping[key]=group
    for group in groups.values():
        insert(by_identity,group['row']['dylib_id']['name'],group)
        for alias in group['aliases']:
            insert(by_source,source_location(alias,roots),group)
            insert(by_rpath,'@rpath/'+PurePosixPath(alias).name,group)
    targets=[]
    for name,row in sorted(binaries.items()):
        if name.startswith('native/'):
            targets.append(dict(source=name,target='bin/'+PurePosixPath(name).name,source_aliases=[name],row=row))
    for digest,group in sorted(groups.items(),key=lambda pair:pair[1]['target']):
        targets.append(dict(source=group['aliases'][0],target=group['target'],source_aliases=group['aliases'],row=group['row']))
    edges,edits=[],[]
    external=set()
    for target in targets:
        source,row,destination=target['source'],target['row'],target['target']
        changes={};expected=[]
        for dependency in row['dependencies']:
            require(len(edges)<100000,'Mac relocation dependency edge budget exceeded')
            old=dependency['name']
            group=None
            if old not in SYSTEM:
                group=by_identity.get(old)
                if group is None:
                    if old.startswith('@rpath/'):
                        group=by_rpath.get(old)
                    elif old.startswith('@loader_path/'):
                        origin=posixpath.dirname(source_location(source,roots))
                        group=by_source.get(posixpath.normpath(origin+'/'+old[len('@loader_path/'):]))
                    elif old.startswith('@executable_path/'):
                        group=by_source.get(posixpath.normpath(roots['native']+'/'+old[len('@executable_path/'):]))
                    elif old.startswith('/'):
                        require(posixpath.normpath(old)==old,'noncanonical absolute Mac dependency')
                        group=by_source.get(old)
                require(group is not None,'unresolved captured Mac dependency: '+source+' -> '+old)
                require(group['row']['dylib_id']['current_version']>=dependency['compatibility_version'],
                        'captured provider is below declared compatibility version floor')
                new=('@executable_path/../' if destination.startswith('bin/') else '@loader_path/')+(
                    group['target'] if destination.startswith('bin/') else PurePosixPath(group['target']).name)
                if old in changes:require(changes[old]==new,'inconsistent Mac install-name replacement')
                changes[old]=new
                resolved=group['target'];resolution='bundled-direct-loader-path'
            else:
                new=old;resolved=None;resolution='macos-system-runtime';external.add(old)
            expected.append(dict(dependency,name=new))
            edges.append(dict(source=destination,dependency=old,replacement=new,target=resolved,
                              mechanism=dependency['mechanism'],resolution=resolution,
                              compatibility_version=dependency['compatibility_version']))
        new_id='@rpath/'+PurePosixPath(destination).name if row['kind']=='dylib' else None
        edits.append(dict(source=source,target=destination,source_aliases=target['source_aliases'],
                          source_sha256=files[source]['sha256'],mode=0o755,
                          install_id=new_id,remove_rpaths=list(dict.fromkeys(row['rpaths'])),
                          changes=[dict(old=old,new=new) for old,new in sorted(changes.items()) if old!=new],
                          expected_dependencies=expected,
                          expected_identity=None if new_id is None else dict(row['dylib_id'],name=new_id),
                          original_metadata=row))
        for interpreter in row['interpreters']:
            require(len(edges)<100000,'Mac relocation dependency edge budget exceeded')
            external.add(interpreter)
            edges.append(dict(source=destination,dependency=interpreter,replacement=interpreter,target=None,
                              mechanism='load-dylinker',resolution='macos-system-interpreter',compatibility_version=None))
    payload=[dict(source=item['source'],target=item['target'],source_aliases=item['source_aliases'],
                  source_sha256=item['source_sha256'],mode=item['mode']) for item in edits]
    for source,entry in sorted(files.items()):
        if source in binaries:continue
        require('/licenses/' in source,'nonruntime/nonnotice payload in Mac snapshot')
        sdk=PurePosixPath(source).parts[1]
        tail=source.split('/licenses/',1)[1]
        if tail.startswith(sdk+'/'):tail=tail[len(sdk)+1:]
        payload.append(dict(source=source,target='licenses/'+sdk+'/'+tail,source_aliases=[source],
                            source_sha256=entry['sha256'],mode=0o644))
    require(len({item['target'].casefold() for item in payload})==len(payload),'Mac package payload target collision')
    outgoing={}
    for edge in edges:outgoing.setdefault(edge['source'],[]).append(edge)
    closures=[]
    for root in ['bin/qupathxx_batch','bin/qupathxx_region']:
        pending,visited,requirements=[root],set(),set()
        while pending:
            current=pending.pop()
            if current in visited:continue
            visited.add(current)
            for edge in outgoing.get(current,[]):
                if edge['target']:pending.append(edge['target'])
                else:requirements.add(edge['dependency'])
        closures.append(dict(entry_point=root,bundled_images=sorted(visited),system_requirements=sorted(requirements)))
    return dict(schema='qpx.macos-native-relocation-plan/1',source_archive=manifest['archive'],
                source_archive_sha256=manifest['archive_sha256'],architecture=architecture,source_roots=roots,
                files=sorted(payload,key=lambda item:item['target']),edits=sorted(edits,key=lambda item:item['target']),
                dependency_edges=edges,entry_points=closures,system_requirements=sorted(external),
                declared_minimum_macos=max((entry['minimum'] for item in edits for entry in item['original_metadata']['minimum_os']),default=0) or None,
                minimum_macos_metadata_complete=all(item['original_metadata']['minimum_os'] for item in edits),
                selected_aliases_deduplicated=True,planned_rpaths=[],
                package_bytes_written=False,apple_tools_executed=False,dynamic_loading_verified=False,
                symbol_abi_verified=False,signing_verified=False,release_ready=False,
                scope='Deterministic native package plan from validated snapshot bytes, explicit source roots and captured identities. '
                      'All dependency kinds require a provider; no missing weak dependency is silently omitted. '
                      'Bundled dependencies become direct executable/loader-relative paths and source RPATHs are removed. '
                      'Declared version-floor admission is not symbol/ABI or runtime availability proof. No binaries changed or loaded.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--architecture',choices=['arm64','x86_64'],required=True)
    parser.add_argument('--source-roots',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    require(args.source_roots.stat().st_size<=65536,'source-root JSON exceeds budget')
    roots=json.loads(args.source_roots.read_text(encoding='utf-8'),object_pairs_hook=inventory.common.unique_object)
    result=create_plan(args.archive,args.architecture,roots)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    inventory.common.publish_new(args.output,inventory.common.canonical_json(result))
    print(json.dumps(dict(files=len(result['files']),binaries=len(result['edits']),release_ready=False)))


if __name__=='__main__':main()
