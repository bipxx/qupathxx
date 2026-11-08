import qupathxx as q


def facts(document: q.Document) -> dict[str, str]:
    document.set_image_properties(q.ObjectProperties({"note": "historical"}, {"Area": 12.5}))
    value: q.ObjectProperties | None = document.image_properties
    return {} if value is None else value.metadata
