import qupathxx as q

data = q.QptmaData()
data.cores = [q.Image.demo()]
q.parse_qptma(7)
q.import_qptma(q.Image.demo(), data)
