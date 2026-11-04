import qupathxx as q
p=q.ObjectProperties({"note":3},{"Area":"bad"})
doc=q.Document(q.Image.demo())
doc.set_object_properties("bad",None)
doc.set_object_properties(1,{"note":"not a native property"})
