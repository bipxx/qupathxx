"""Bounded contingency-table label matching for real-slide characterization."""
import numpy as np
from scipy.optimize import linear_sum_assignment


def metrics(reference,native,tolerance,overlap_budget=64*1024*1024):
    reference,native=np.asarray(reference),np.asarray(native)
    if (reference.shape!=native.shape or reference.ndim!=2 or
            reference.dtype.kind not in 'iu' or native.dtype.kind not in 'iu' or
            np.any(reference<0) or np.any(native<0)):
        raise ValueError('label canvases must be equal nonnegative integer 2D arrays')
    rids,nids=np.unique(reference[reference>0]),np.unique(native[native>0])
    nr,nn=len(rids),len(nids)
    # Account for the integer overlaps, float unions, IoUs and relation scratch.
    if nr and nn and nr>overlap_budget//64//nn:
        raise ValueError('label matching contingency table exceeds its budget')

    def moments(labels,ids):
        y,x=np.nonzero(labels)
        indices=np.searchsorted(ids,labels[y,x])
        area=np.bincount(indices,minlength=len(ids)).astype(np.int64)
        sx=np.bincount(indices,weights=x,minlength=len(ids))
        sy=np.bincount(indices,weights=y,minlength=len(ids))
        return area,np.divide(sx,area,out=np.zeros(sx.shape,dtype=float),where=area>0),np.divide(sy,area,out=np.zeros(sy.shape,dtype=float),where=area>0)

    ra,rx,ry=moments(reference,rids)
    na,nx,ny=moments(native,nids)
    both=(reference>0)&(native>0)
    if np.any(both):
        pairs=np.searchsorted(rids,reference[both])*nn+np.searchsorted(nids,native[both])
        overlap=np.bincount(pairs,minlength=nr*nn).reshape(nr,nn)
    else:
        overlap=np.zeros((nr,nn),dtype=np.int64)
    union=ra[:,None]+na[None,:]-overlap
    iou=np.divide(overlap,union,out=np.zeros_like(overlap,dtype=float),where=union>0)
    rows,columns=linear_sum_assignment(-iou)
    matches=[]
    for r,n in zip(rows,columns):
        if iou[r,n]<tolerance['match_min_iou']:
            continue
        matches.append(dict(reference_label=int(rids[r]),native_label=int(nids[n]),iou=float(iou[r,n]),
                            centroid_error_px=float(np.hypot(rx[r]-nx[n],ry[r]-ny[n])),
                            reference_pixels=int(ra[r]),native_pixels=int(na[n])))
    denominator=np.minimum(ra[:,None],na[None,:])
    relation=(overlap>=tolerance['relation_min_pixels'])&(overlap>=tolerance['relation_min_fraction']*denominator)
    intersection=int(np.count_nonzero(both))
    foreground_union=int(np.count_nonzero((reference>0)|(native>0)))
    total=int(ra.sum()+na.sum())
    return dict(reference_count=nr,native_count=nn,matches=matches,
                unmatched_reference=nr-len(matches),unmatched_native=nn-len(matches),
                split_reference_labels=[int(rids[i]) for i in np.flatnonzero(relation.sum(axis=1)>1)],
                merged_native_labels=[int(nids[i]) for i in np.flatnonzero(relation.sum(axis=0)>1)],
                foreground_iou=intersection/foreground_union if foreground_union else 1.,
                foreground_dice=2*intersection/total if total else 1.)
