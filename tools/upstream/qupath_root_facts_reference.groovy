// QuPath 0.7.0 root historical-facts reference; one output directory argument.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.io.GsonTools
import qupath.lib.objects.PathObjects
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs

def output=Path.of(args[0]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class)
def server=ImageServerProvider.buildServer(output.resolve('source.ome.tif').toString(),BufferedImage.class)
def entry=project.addImage(server.getBuilder());entry.setImageName('root facts')
def data=new ImageData(server);def root=data.getHierarchy().getRootObject()
root.getMetadata().put('specimen','synthetic α 🚀')
root.getMetadata().put('note','historical root; not ROI or computed image analysis')
['Root area':12.5,'missing':Double.NaN,'positive infinity':Double.POSITIVE_INFINITY,'negative infinity':Double.NEGATIVE_INFINITY,'negative zero':-0.0d].each {key,value -> root.getMeasurementList().put(key,value)}
def child=PathObjects.createAnnotationObject(ROIs.createRectangleROI(2,3,10,12,ImagePlane.getDefaultPlane()))
child.setName('Child');child.getMetadata().put('note','child facts');child.getMeasurementList().put('Root area',3.5)
root.addChildObject(child);entry.saveImageData(data);project.syncChanges()
def measured=[:]
root.getMeasurementList().getNames().each { key ->
    def value=root.getMeasurementList().get(key)
    measured[key]=Double.isNaN(value)?'NaN':Double.isInfinite(value)?(value>0?'Infinity':'-Infinity'):value
}
def reference=[metadata:new TreeMap(root.getMetadata()),measurements:measured,objects:1,child:[metadata:new TreeMap(child.getMetadata()),measurement:child.getMeasurementList().get('Root area'),area:child.getROI().getArea()]]
Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).toJson(reference)+'\n')
server.close()
