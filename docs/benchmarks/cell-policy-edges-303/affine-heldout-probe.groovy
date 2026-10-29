import java.awt.geom.AffineTransform
import java.awt.geom.Point2D
import java.nio.file.Path
import java.nio.file.Files
import qupath.lib.io.GsonTools
def root=Path.of(args[0])
def cases=GsonTools.getInstance().fromJson(Files.readString(root.resolve('affine-heldout-requests.json')),List.class)
def results=cases.collect { item ->
    def transform=new AffineTransform()
    transform.scale(1d/item.step,1d/item.step);transform.translate(-item.origin[0],-item.origin[1])
    def inverse=transform.createInverse();def center=inverse.transform(new Point2D.Double(.5,.5),null)
    [name:item.name,first_center:[center.x,center.y]]
}
Files.writeString(root.resolve('affine-heldout-reference.json'),GsonTools.getInstance(true).toJson(results)+'\n')
