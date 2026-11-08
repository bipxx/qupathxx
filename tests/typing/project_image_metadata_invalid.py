from qupathxx import Document, Image, Project
project=Project()
image=project.add_document(Document(Image.demo()),"Entry")
project.set_image_metadata(image,{"key":1})
project.images[0].metadata={}
metadata: list[str]=project.image_metadata(image)
