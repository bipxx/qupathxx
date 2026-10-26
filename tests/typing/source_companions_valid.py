import qupathxx as q


def source_files(source: q.ReaderSource) -> list[q.FileVersion]:
    files: list[q.FileVersion]=source.companions
    primary=source.primary
    if primary is not None:
        files.insert(0,primary)
        path: str=primary.path
        assert path
    return files


def observed_files(server: q.ImageServer) -> list[q.FileVersion]:
    observed=server.reader_source
    return [] if observed is None else source_files(observed)


def provenance_files(provenance: q.AnalysisProvenance) -> list[q.FileVersion]:
    observed=provenance.reader_source
    return [] if observed is None else source_files(observed)
