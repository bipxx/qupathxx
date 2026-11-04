import qupathxx as q


def invalid(document: q.Document) -> None:
    q.copy_objects(document)
    q.copy_objects(document, ["object"])
    q.paste_objects(document, 123)
    q.paste_objects(document, "{}", plane=1)
