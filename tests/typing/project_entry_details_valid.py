from qupathxx import Document,Image,Project,ProjectImage
project=Project();image=project.add_document(Document(Image.demo()),"Entry")
project.set_image_details(image,description="text",tags=["b","a","b",""])
description: str | None=project.image_description(image)
tags: list[str]=project.image_tags(image)
entry: ProjectImage=project.images[0]
owned: str | None=entry.description
copy: list[str]=entry.tags
project.set_image_details(image)
