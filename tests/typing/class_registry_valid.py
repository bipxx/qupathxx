from qupathxx import Document, ObjectClass, Project


def edit_classes(doc: Document, project: Project, image_id: int) -> tuple[list[str], int | None]:
    root = ObjectClass("Root", color_argb=-2146290602)
    child = ObjectClass("Root: Positive", color_argb=None, parent=root.name)
    doc.replace_class_registry([root, child])
    doc.set_class_definition(child)
    path: list[str] = doc.class_path(child.name)
    owned: list[ObjectClass] = project.classes
    defaults: list[str] = project.class_defaults
    project.set_class_registry(owned + [root, child], defaults + [child.name, child.name])
    project.apply_class_defaults(image_id)
    raw: int | None = doc.object_class(child.name).color_argb
    leaf: str = child.leaf_name
    parent: str = child.parent
    assert leaf and parent
    return path, raw
