import qupathxx as q
props=q.ObjectProperties(locked="bad",color_rgb="bad",class_probability="bad")
doc=q.Document(q.Image.demo())
doc.set_object_edit_state("bad",True)
props.with_edit_state(locked=False,color_rgb="bad")
