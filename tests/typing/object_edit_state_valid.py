import qupathxx as q
props=q.ObjectProperties({"note":"saved"},{"Synthetic":3.5},locked=True,color_rgb=0x123456,class_probability=.875)
changed=props.with_edit_state(locked=False,color_rgb=None,class_probability=None)
doc=q.Document(q.Image.demo());id=doc.add_rectangle(q.Rect(1,2,3,4))
doc.set_object_properties(id,changed)
doc.set_object_edit_state(id,True,0x123456,.875)
locked:bool=doc.annotation(id).locked
color:int|None=props.color_rgb
probability:float|None=props.class_probability
