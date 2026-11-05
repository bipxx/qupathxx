from qupathxx import Document,Image,Project
project=Project();image=project.add_document(Document(Image.demo()),"Entry")
project.set_image_details(image,tags=[7])
project.images[0].description="forbidden"
tags: str=project.image_tags(image)
