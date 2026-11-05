from qupathxx import Document, Image, Project, ProjectImage
project=Project()
image=project.add_document(Document(Image.demo()),"Entry")
project.set_image_metadata(image,{"case":"synthetic","empty":""})
metadata: dict[str,str]=project.image_metadata(image)
entry: ProjectImage=project.images[0]
copy: dict[str,str]=entry.metadata
project.set_image_metadata(image)
