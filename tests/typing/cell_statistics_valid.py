from qupathxx import CellResult, CellIntensityStatistics, CompartmentIntensityStatistics, ChannelIntensityStatistics, CellTableOptions, GuiOptions, ObjectFeatureTable

def inspect(result: CellResult, table: ObjectFeatureTable) -> None:
    value: CellIntensityStatistics | None = result.intensity_statistics(1)
    format: str | None = result.intensity_statistics_format
    if value is not None:
        compartment: CompartmentIntensityStatistics = value.nucleus
        channels: ChannelIntensityStatistics = compartment.channels
        sd: list[float] | None = channels.sample_standard_deviation
    selected: CellIntensityStatistics | None = table.cell_intensity_statistics(0)
    options = CellTableOptions()
    options.include_intensity_statistics = True
    gui = GuiOptions()
    gui.cell_table_intensity_statistics = True
