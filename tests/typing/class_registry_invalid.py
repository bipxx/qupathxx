from qupathxx import Document, ObjectClass, Project


def invalid_classes(doc: Document, project: Project) -> None:
    ObjectClass("Invalid", color_argb="red")
    doc.set_class_definition("Root")
    project.set_class_registry(["Root"], ["Root"])
