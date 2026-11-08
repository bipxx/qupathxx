import qupathxx as q
plane: q.ObjectPlane=q.ObjectPlane(c=1,z=2,t=0)
doc=q.Document(q.Image.demo())
obj: int=doc.add_rectangle(q.Rect(1,2,3,4))
doc.set_object_plane(obj,plane)
retained: q.ObjectPlane=doc.annotation(obj).plane
active: q.ObjectPlane=doc.active_plane
visible: bool=doc.object_visible(obj)
edit: q.ObjectEdit=q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(1,2,3,4)),plane=q.ObjectPlane())
doc.execute([q.ObjectEdit.plane(obj,plane),edit])
