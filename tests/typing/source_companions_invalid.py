import qupathxx as q

def invalid(source: q.ReaderSource) -> None:
    source.companions=[]
    source.companions.append(None)
    print(source.primary.path)
