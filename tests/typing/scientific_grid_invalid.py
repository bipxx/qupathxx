import qupathxx as q

axis = q.ScientificGridAxis(0, 3, 2)
axis.origin = 9
grid = q.ScientificGrid(8, 8, axis, axis)
grid.width = 9
axis.center("zero")
q.GridFraction(1.5, 2)
q.ScientificRgbGrid()
q.read_scientific_rgb(None, grid)
