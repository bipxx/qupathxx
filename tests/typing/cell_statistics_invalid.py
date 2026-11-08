from qupathxx import CellResult, ObjectFeatureTable

def misuse(result: CellResult, table: ObjectFeatureTable) -> None:
    result.intensity_statistics("first")
    table.cell_intensity_statistics("first")
    result.intensity_statistics_format = "made-up"
