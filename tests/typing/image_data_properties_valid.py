import qupathxx as q


def properties(doc: q.Document) -> dict[str, object]:
    doc.set_image_data_properties(q.ImageDataProperties({'exact': 9007199254740993, 'nested': [True, None, {'score': 3.5}]}))
    value: q.ImageDataProperties | None = doc.image_data_properties
    return value.values if value is not None else {}
