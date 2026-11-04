import qupathxx as q
q.ObjectPlane(c="red")
doc=q.Document(q.Image.demo())
doc.set_object_plane(1,"Z1")
doc.object_visible("one")
q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(1,2,3,4)),plane="Z1")
