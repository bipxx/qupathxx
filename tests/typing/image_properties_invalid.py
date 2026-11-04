import qupathxx as q


def invalid(document: q.Document) -> None:
    document.set_image_properties({"note": "map"})
    value: q.ObjectProperties = document.image_properties
    document.image_properties = None
