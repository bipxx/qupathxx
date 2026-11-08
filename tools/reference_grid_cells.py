"""Independent exact small-grid LoG/expansion cases; no native imports."""
import argparse
from fractions import Fraction as F
import heapq
import json
from pathlib import Path
import random


def neighbours(i,w,h):
    x,y=i%w,i//w
    return ([i-w] if y else [])+([i-1] if x else [])+([i+1] if x+1<w else [])+([i+w] if y+1<h else [])


def curvature(case):
    w,h=case['width'],case['height'];v=list(map(F,case['score']));valid=case['valid'];sx,sy=map(F,case['step'])
    out=[]
    for i in range(w*h):
        x,y=i%w,i//w
        def value(j):return v[j] if valid[j] else v[i]
        dx=(2*v[i]-value(i-1 if x else i)-value(i+1 if x+1<w else i))/(sx*sx)
        dy=(2*v[i]-value(i-w if y else i)-value(i+w if y+1<h else i))/(sy*sy)
        out.append(int(bool(valid[i]) and dx+dy>0))
    return out


def expand(case):
    w,h=case['width'],case['height'];sx,sy=map(F,case['step']);radius=F(case['radius'])
    seeds=case['seeds'];domain=case['domain'];labels=seeds.copy();sites=[i for i in range(w*h) if seeds[i]]
    if not sites or radius==0:return labels
    # Exhaustive rational distances, independent of the native separable EDT.
    d=[min(((F(i%w-j%w)*sx)**2+(F(i//w-j//w)*sy)**2 for j in sites)) for i in range(w*h)]
    visited=[not domain[i] or bool(seeds[i]) or d[i]>=radius*radius for i in range(w*h)]
    frontier=[];sequence=0
    def enqueue(i):
        nonlocal sequence
        if visited[i]:return
        visited[i]=True;heapq.heappush(frontier,(d[i],sequence,i));sequence+=1
    for i in range(w*h):
        if any(seeds[j] for j in neighbours(i,w,h)):enqueue(i)
    while frontier:
        _,_,i=heapq.heappop(frontier);owners={labels[j] for j in neighbours(i,w,h) if labels[j]}
        if len(owners)!=1:continue
        labels[i]=owners.pop()
        for j in neighbours(i,w,h):enqueue(j)
    return labels


def reference():
    rng=random.Random(164);logs=[];bodies=[]
    for k in range(32):
        w,h=3+k%3,2+k%4;n=w*h
        c=dict(width=w,height=h,score=[rng.randrange(-4,8) for _ in range(n)],valid=[int(rng.randrange(5)!=0) for _ in range(n)],step=[['1/4','1/2','1','2'][k%4],['1/2','1','2','4'][(k//4)%4]])
        c['positive']=curvature(c);logs.append(c)
    for k in range(128):
        w,h=3+k%3,2+k%4;n=w*h
        seeds=[0]*n
        for label,at in enumerate(rng.sample(range(n),1+k%3),1):seeds[at]=label
        domain=[int(bool(seeds[i]) or rng.randrange(5)!=0) for i in range(n)]
        c=dict(width=w,height=h,seeds=seeds,domain=domain,step=[['1/4','1/2','1','2'][k%4],['1/2','1','2','4'][(k//4)%4]],radius=['0','1/4','3/4','3/2','3','5'][k%6])
        c['labels']=expand(c);bodies.append(c)
    return dict(schema='qpx.grid-cell-kernel-reference/1',scope='Exact zero-sigma metric LoG sign and strict-radius watershed expansion; not scientific detector acceptance',curvature=logs,expansion=bodies)


def native_header(data):
    def array(items):return '{'+','.join(str(x) for x in items)+'}'
    def number(text):return repr(float(F(text)))
    out=['#pragma once','#include <vector>','namespace grid_cell_reference {',
         'struct Curvature {unsigned w,h;double sx,sy;std::vector<double> score;std::vector<unsigned char> valid,positive;};',
         'inline const std::vector<Curvature> curvature={']
    for c in data['curvature']:
        out.append('{'+f"{c['width']},{c['height']},{number(c['step'][0])},{number(c['step'][1])},"+','.join(array(c[key]) for key in ('score','valid','positive'))+'},')
    out+=['};','struct Expansion {unsigned w,h;double sx,sy,radius;std::vector<unsigned> seeds;std::vector<unsigned char> domain;std::vector<unsigned> labels;};','inline const std::vector<Expansion> expansion={']
    for c in data['expansion']:
        out.append('{'+f"{c['width']},{c['height']},{number(c['step'][0])},{number(c['step'][1])},{number(c['radius'])},"+','.join(array(c[key]) for key in ('seeds','domain','labels'))+'},')
    return '\n'.join(out+['};','}',''])


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--native-header',type=Path,required=True);a=p.parse_args()
    if a.output.exists() or a.native_header.exists():p.error('reference outputs must not exist')
    data=reference()
    a.output.write_bytes((json.dumps(data,indent=2)+'\n').encode('utf-8'))
    a.native_header.write_bytes(native_header(data).encode('utf-8'))
