"""Independent library transforms and brute-force expansion for small fixtures."""
import numpy as np
import qupathxx as q
from scipy import ndimage as ndi
from skimage.morphology import local_maxima
from skimage.segmentation import watershed


def run(rgb, transform, parameters, domain=None):
    p = parameters
    domain = np.ones(rgb.shape[:2], dtype=bool) if domain is None else domain
    od = np.maximum(0, np.log10(np.asarray(transform.white)/np.maximum(rgb.astype(float), 1)))
    stain = p.feature != q.TissueFeature.OPTICAL_DENSITY_SUM
    channels = (np.linalg.solve(np.asarray(transform.stains).T, od.reshape(-1, 3).T).T.reshape(rgb.shape)
                if stain else od).astype(np.float32).astype(float)
    score = channels[..., [q.TissueFeature.STAIN_1, q.TissueFeature.STAIN_2, q.TissueFeature.STAIN_3].index(p.feature)] if stain else channels.sum(axis=2)
    if p.sigma:
        kwargs = dict(sigma=p.sigma, radius=int(np.ceil(3*p.sigma)), mode="constant", cval=0)
        numerator = ndi.gaussian_filter(np.where(domain, score, 0), **kwargs)
        denominator = ndi.gaussian_filter(domain.astype(float), **kwargs)
        score = np.divide(numerator, denominator, out=np.zeros_like(score), where=denominator > 0)
    mask = domain & (score >= p.threshold)
    distance = ndi.distance_transform_edt(np.pad(mask, 1))[1:-1, 1:-1]
    connectivity = 1 if p.connectivity == 4 else 2
    peaks = local_maxima(distance, connectivity=connectivity) & mask
    groups, count = ndi.label(peaks, ndi.generate_binary_structure(2, connectivity))
    candidates = []
    for label in range(1, count+1):
        index = int(np.flatnonzero(groups.ravel() == label)[0])
        if distance.ravel()[index] >= p.min_peak_radius:
            candidates.append(index)
    candidates.sort(key=lambda i: (-distance.ravel()[i], i))
    kept = []
    for index in candidates:
        y, x = divmod(index, rgb.shape[1])
        if all((y-j//rgb.shape[1])**2+(x-j % rgb.shape[1])**2 >= p.min_seed_distance**2 for j in kept):
            kept.append(index)
    kept.sort()
    markers = np.zeros(mask.shape, dtype=np.int32)
    for label, index in enumerate(kept, 1):
        markers.ravel()[index] = label
    raw = watershed(-distance, markers, mask=mask, connectivity=connectivity)
    counts = np.bincount(raw.ravel(), minlength=len(kept)+1)
    remap = np.zeros(len(kept)+1, dtype=np.uint32)
    dense = 0
    for label in range(1, len(kept)+1):
        if counts[label] >= p.min_nucleus_pixels and (not p.max_nucleus_pixels or counts[label] <= p.max_nucleus_pixels):
            dense += 1
            remap[label] = dense
    nuclei = remap[raw]
    bodies = expand(nuclei, domain, p.cell_expansion)
    return nuclei, bodies, kept, remap, channels, len(candidates)


def expand(nuclei, domain, radius):
    points = np.argwhere(nuclei != 0)
    bodies = np.zeros_like(nuclei)
    if not len(points):
        return bodies
    for chunk in np.array_split(np.argwhere(domain), max(1, int(domain.sum())//128)):
        delta = chunk[:, None, :]-points[None, :, :]
        distances = (delta*delta).sum(axis=2)
        nearest = distances.argmin(axis=1)  # Equal distances choose the first raster site.
        accepted = distances[np.arange(len(chunk)), nearest] <= radius*radius
        ys, xs = chunk[accepted].T
        sites = points[nearest[accepted]]
        bodies[ys, xs] = nuclei[sites[:, 0], sites[:, 1]]
    return bodies
