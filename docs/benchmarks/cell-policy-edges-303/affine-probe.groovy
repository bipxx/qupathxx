import java.awt.geom.AffineTransform
import java.awt.geom.Point2D
import java.nio.file.Path
import java.nio.file.Files
import qupath.lib.io.GsonTools
def root=Path.of(args[0])
def contract=GsonTools.getInstance().fromJson(Files.readString(root.resolve('capture1/contract.json')),Map.class)
def cases=contract.cases.collect { item ->
    double step=(item.requested_pixel_size_um ?: .5)/.5
    def origin=item.roi ?: [0,0,256,192]
    def transform=new AffineTransform()
    transform.scale(1d/step,1d/step);transform.translate(-(origin[0] as double),-(origin[1] as double))
    def inverse=transform.createInverse()
    def matrix=new double[6];inverse.getMatrix(matrix)
    def center=inverse.transform(new Point2D.Double(.5,.5),null)
    [name:item.name,step:step,origin:origin.take(2),inverse:matrix,first_center:[center.x,center.y]]
}
Files.writeString(root.resolve('affine-reference.json'),GsonTools.getInstance(true).toJson(cases)+'\n')
