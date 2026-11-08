import qupathxx as q
p=q.ObjectProperties({"note":"saved"},{"Area":3.5})
doc=q.Document(q.Image.demo())
id=doc.add_rectangle(q.Rect(1,2,3,4))
doc.set_object_properties(id,p)
doc.execute([q.ObjectEdit.properties(id,None)])
metadata:dict[str,str]=p.metadata
measurements:dict[str,float]=p.measurements
