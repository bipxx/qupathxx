import qupathxx as q

doc = q.Document(q.Image.demo())
doc.insert_tma_row("wrong index")
doc.insert_tma_column(0, bounds=[q.Point(1, 2)])
q.ViewerSession("project").remove_tma_row(1, 0, expected_revision="wrong revision")
