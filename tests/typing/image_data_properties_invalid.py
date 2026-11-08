import qupathxx as q


def invalid(doc: q.Document) -> None:
    doc.set_image_data_properties({'not': 'a native properties value'})
    value: q.ImageDataProperties = doc.image_data_properties
    doc.image_data_properties = None
