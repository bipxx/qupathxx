import qupathxx as q
project=q.Project()
metadata: dict[str,str]=project.metadata
project.set_metadata({"project":"research"})
project.set_metadata()
