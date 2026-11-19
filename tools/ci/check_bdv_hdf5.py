# SPDX-License-Identifier: AGPL-3.0-only
"""Compare the native BDV HDF5 provider with a pinned pybdv project."""

from check_bdv_n5 import main


HDF5 = "dataset.h5"
CELLS_PATH = "t00000/s00/0/cells"
LEVEL1_CELLS_PATH = "t00000/s00/1/cells"
FIRST_CHUNK = (0, 0, 0)


def options(parser):
    parser.add_argument("--hdf5-mutations", action="store_true",
                        help="Use the isolated h5py producer environment to author storage mutation controls")


def check_storage(args, project, variant, raw, identities):
    missing = variant("missing-hdf5-source")
    (missing / HDF5).unlink()
    raw("missing-hdf5-source", missing, status=2)
    truncated = variant("truncated-hdf5-source")
    file = truncated / HDF5
    file.write_bytes(file.read_bytes()[:7])
    raw("truncated-hdf5-source", truncated, status=2)
    if args.hdf5_mutations:
        check_mutations(project, variant, raw, identities)


def check_mutations(project, variant, raw, identities):
    import h5py

    def fail(name, mutate):
        source = variant(name)
        with h5py.File(source / HDF5, "r+") as file:
            mutate(file)
        raw(name, source, status=2)

    def replace(file, path, *, dtype=None, shape=None, scaleoffset=None):
        original = file[path]
        data = original[:]
        chunks = original.chunks
        del file[path]
        if shape is not None:
            data = data[tuple(slice(0, size) for size in shape)]
        if dtype is not None:
            data = data.astype(dtype)
        file.create_dataset(path, data=data, chunks=chunks, compression="gzip",
                            compression_opts=5, scaleoffset=scaleoffset)

    def link(file, target):
        del file[CELLS_PATH]
        file[CELLS_PATH] = target

    def corrupt_chunk(file):
        cells = file[CELLS_PATH]
        mask, payload = cells.id.read_direct_chunk(FIRST_CHUNK)
        assert not mask and len(payload) >= 8
        cells.id.write_direct_chunk(FIRST_CHUNK, bytes(len(payload)), filter_mask=0)

    fail("explicit-hdf5-datatype",
         lambda file: file["s00"].attrs.__setitem__("dataType", "int16"))
    fail("wrong-hdf5-cell-datatype", lambda file: replace(file, CELLS_PATH, dtype="i4"))
    fail("hdf5-level-dimensions-disagree",
         lambda file: replace(file, LEVEL1_CELLS_PATH, shape=(3, 4, 5)))
    fail("hdf5-z-downsampling", lambda file: file["s00/resolutions"].__setitem__((1, 2), 2))
    fail("hdf5-subdivisions-disagree", lambda file: file["s00/subdivisions"].__setitem__((0, 0), 3))
    fail("hdf5-unadmitted-filter", lambda file: replace(file, CELLS_PATH, scaleoffset=0))
    fail("hdf5-soft-cell-link",
         lambda file: link(file, h5py.SoftLink("/t00001/s00/0/cells")))
    fail("hdf5-external-cell-link",
         lambda file: link(file, h5py.ExternalLink(str(project / HDF5), "/" + CELLS_PATH)))
    fail("hdf5-corrupt-deflate-chunk", corrupt_chunk)

    source = variant("sparse-hdf5-chunk")
    with h5py.File(source / HDF5, "r+") as file:
        existing = file[CELLS_PATH]
        del file[CELLS_PATH]
        cells = file.create_dataset(CELLS_PATH, shape=existing.shape, dtype=existing.dtype,
                                    chunks=existing.chunks, compression="gzip", compression_opts=5)
        for region in existing.iter_chunks():
            if any(axis.start for axis in region):
                cells[region] = existing[region]
        assert cells.id.get_chunk_info_by_coord(FIRST_CHUNK).size == 0
    raw("sparse-hdf5-chunk", source, width=4, height=3, pixels=(0,) * 12)
    assert identities[source] != identities[project]


if __name__ == "__main__":
    main(backend="hdf5", describe=__doc__, add_backend_options=options, check_backend=check_storage)
