import qupathxx as q


def transfer(source: q.Document, target: q.Document, ids: list[int], plane: q.ObjectPlane | None) -> list[int]:
    snapshot: q.InterchangeReport = q.copy_objects(source, ids, include_descendants=True, max_bytes=1024*1024)
    text: str = snapshot.text
    result: q.InterchangeReport = q.paste_objects(target, text, plane=plane)
    return result.imported_ids
