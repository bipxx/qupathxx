// QuPath 0.7.0 synthetic saved-data reference. One output-directory argument.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.io.GsonTools
import qupath.lib.objects.PathObjects
import qupath.lib.objects.classes.PathClass
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs
import qupath.lib.geom.Point2
import qupath.lib.plugins.workflow.DefaultScriptableWorkflowStep
import org.locationtech.jts.io.WKTReader
import qupath.lib.roi.GeometryTools

def output=Path.of(args[0]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class)
def cls=PathClass.fromString('Tumor');cls.setColor(0xff2030)
def derived=PathClass.getInstance(cls,'Positive',0x008800)
project.setPathClasses([cls,derived])
def plane=ImagePlane.getDefaultPlane()
def summaries=[]
['geometry','losses','plane'].each { label ->
    def server=ImageServerProvider.buildServer(output.resolve('source.ome.tif').toString(),BufferedImage.class)
    def entry=project.addImage(server.getBuilder());entry.setImageName(label)
    def data=new ImageData(server)
    def objects=[]
    if(label=='geometry') {
        def rectangle=PathObjects.createAnnotationObject(ROIs.createRectangleROI(2,2,50,40,plane),cls);rectangle.setName('Parent α 🚀')
        def child=PathObjects.createAnnotationObject(ROIs.createEllipseROI(5,5,12,8,plane),cls);child.setName('Ellipse');rectangle.addChildObject(child)
        objects.add(rectangle)
        def roi=ROIs.createPolygonROI([new Point2(20,2),new Point2(30,3),new Point2(25,10)],plane)
        def polygon=PathObjects.createAnnotationObject(roi);polygon.setName('Polygon');roi.getArea();objects.add(polygon)
        def line=PathObjects.createAnnotationObject(ROIs.createPolylineROI([new Point2(1,1),new Point2(10,3),new Point2(12,10)],plane));line.setName('Line');objects.add(line)
        def points=PathObjects.createAnnotationObject(ROIs.createPointsROI([new Point2(2,3),new Point2(4,6)],plane));points.setName('Points');objects.add(points)
        def geometry=new WKTReader().read('MULTIPOLYGON (((1 20,10 20,10 30,1 30,1 20),(3 22,3 24,5 24,5 22,3 22)),((20 20,25 20,25 25,20 25,20 20)))')
        def holes=PathObjects.createAnnotationObject(GeometryTools.geometryToROI(geometry,plane));holes.setName('Holes');holes.getROI().getArea();objects.add(holes)
        def detection=PathObjects.createDetectionObject(ROIs.createRectangleROI(30,20,4,4,plane),cls);detection.setName('Detection');objects.add(detection)
        def cell=PathObjects.createCellObject(ROIs.createEllipseROI(40,20,8,10,plane),ROIs.createEllipseROI(42,23,3,4,plane),cls,null);cell.setName('Cell');objects.add(cell)
    } else if(label=='losses') {
        def a=PathObjects.createAnnotationObject(ROIs.createRectangleROI(1,1,10,10,plane),derived)
        a.setName('Lossy');a.setLocked(true);a.setColor(0x123456);a.getMetadata().put('note','saved note');a.getMeasurementList().put('Synthetic',3.5)
        data.setImageType(ImageData.ImageType.BRIGHTFIELD_H_E)
        data.getHistoryWorkflow().addStep(new DefaultScriptableWorkflowStep('Synthetic history',"println 'never execute during import'"))
        data.setProperty('synthetic','value');objects.add(a)
    } else {
        def a=PathObjects.createAnnotationObject(ROIs.createRectangleROI(1,1,10,10,ImagePlane.getPlane(1,0)),cls);a.setName('Other Z');objects.add(a)
    }
    objects.each {data.getHierarchy().getRootObject().addChildObject(it)}
    entry.saveImageData(data)
    def rows=[]
    def visit
    visit={ a,parent ->
        def roi=a.getROI();def nucleus=a.isCell()?a.getNucleusROI():null
        rows.add([id:a.getID().toString(),parent:parent,name:a.getName(),kind:a.isCell()?'cell':a.isDetection()?'detection':'annotation',roi:roi.getClass().getSimpleName(),area:roi.getArea(),bounds:[roi.getBoundsX(),roi.getBoundsY(),roi.getBoundsWidth(),roi.getBoundsHeight()],nucleus_area:nucleus==null?null:nucleus.getArea(),classification:a.getPathClass()?.toString()])
        a.getChildObjects().each {visit(it,a.getID().toString())}
    }
    data.getHierarchy().getRootObject().getChildObjects().each {visit(it,null)}
    summaries.add([entry_id:entry.getID(),name:label,objects:rows])
    server.close()
}
project.syncChanges()
Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(summaries)+'\n')
