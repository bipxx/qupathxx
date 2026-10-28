import qupathxx as q

def submit(session: q.ViewerSession, transform: q.StainTransform) -> None:
    q.GuiOptions().cell_nominal = q.GridCellSampling()
    session.nominal_cells(1, 1, transform, q.NominalCellSampling(q.RgbNominalRequest(0, 0, 17, 13)))
    session.nominal_cells(1, 1, transform, retain_result='yes')
