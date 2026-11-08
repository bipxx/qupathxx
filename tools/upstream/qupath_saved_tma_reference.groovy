// Native saved-data TMA contract. Existing QPTMA text behavior is not recaptured.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.objects.PathObjects
import qupath.lib.objects.classes.PathClass
import qupath.lib.objects.hierarchy.DefaultTMAGrid
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs
import qupath.lib.io.GsonTools

def output=Path.of(args[0]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class);def summaries=[]
def plane=ImagePlane.getDefaultPlane();def cls=PathClass.fromString('Tumor');cls.setColor(0xff2030);project.setPathClasses([cls])
['grid','legacy-case','ragged-grid','duplicate-member','orphan-core','absent-member','empty-grid','unlisted-core'].each {label ->
    def server=ImageServerProvider.buildServer(output.resolve('source.ome.tif').toString(),BufferedImage.class)
    def entry=project.addImage(server.getBuilder());entry.setImageName(label);def data=new ImageData(server)
    def names=['Core α','Replicate','Replicate','Skipped','Comma, core','Unassigned']
    def cases=['case-one','case-one','','case α','comma, value','']
    def cores=(0..<6).collect {i ->
        int row=i.intdiv(3);int col=i%3
        def core=PathObjects.createTMACoreObject(5.25+col*18.5+row*0.25,5.5+row*20.25+col*0.125,8.5+i*0.25,10.25+i*0.25,i==1 || i==4)
        core.setName(names[i]);if(!cases[i].isEmpty())core.setCaseID(cases[i])
        if(label=='legacy-case' && i==5)core.getMetadata().put('Unique ID','legacy α')
        core.getMetadata().put('note','core-'+i);core.getMeasurementList().put('Imported score',i+0.5)
        if(i==0) {core.setPathClass(cls);core.setColor(0x123456);core.setLocked(true)}
        core
    }
    def root=data.getHierarchy().getRootObject()
    def members=cores
    if(label=='empty-grid')members=[]
    else if(label=='ragged-grid')members=cores.take(5)
    else if(label=='duplicate-member')members=[cores[0],cores[0]]+cores.drop(2)
    if(label!='unlisted-core')data.getHierarchy().setTMAGrid(DefaultTMAGrid.create(members,3))
    if(label in ['grid','legacy-case']) {
        def b=cores[0].getROI();double x=b.getBoundsX()+b.getBoundsWidth()/2,y=b.getBoundsY()+b.getBoundsHeight()/2
        def child=PathObjects.createAnnotationObject(ROIs.createRectangleROI(x-1,y-1,2,2,plane),cls);child.setName('Nested annotation')
        def point=PathObjects.createAnnotationObject(ROIs.createPointsROI(x,y,plane));point.setName('Grandchild');child.addChildObject(point);cores[0].addChildObject(child)
        b=cores[2].getROI();x=b.getBoundsX()+b.getBoundsWidth()/2;y=b.getBoundsY()+b.getBoundsHeight()/2
        def detection=PathObjects.createDetectionObject(ROIs.createRectangleROI(x-1,y-1,2,2,plane),cls);detection.setName('Nested detection');cores[2].addChildObject(detection)
        b=cores[3].getROI();x=b.getBoundsX()+b.getBoundsWidth()/2;y=b.getBoundsY()+b.getBoundsHeight()/2
        def cell=PathObjects.createCellObject(ROIs.createEllipseROI(x-2,y-2,4,4,plane),ROIs.createEllipseROI(x-0.5,y-0.5,1,1,plane),cls,null);cell.setName('Nested cell');cores[3].addChildObject(cell)
        root.clearChildObjects();root.addChildObjects(new ArrayList(cores.reverse()))
        def outside=PathObjects.createAnnotationObject(ROIs.createRectangleROI(1,41,3,3,plane));outside.setName('Outside');root.addChildObject(outside)
    }
    if(label=='orphan-core' || label=='unlisted-core') {
        def orphan=PathObjects.createTMACoreObject(1,41,3,3,false);orphan.setName('Orphan core');root.addChildObject(orphan)
    }
    if(label=='absent-member')root.removeChildObject(cores[5])
    entry.saveImageData(data);def restored=entry.readImageData();def grid=restored.getHierarchy().getTMAGrid()
    def describe={o ->
        def r=o.getROI();def nucleus=o.isCell()?o.getNucleusROI():null
        [id:o.getID().toString(),parent:o.getParent()!=null && !o.getParent().isRootObject()?o.getParent().getID().toString():null,
         name:o.getName(),kind:o.isTMACore()?'tma':o.isCell()?'cell':o.isDetection()?'detection':'annotation',bounds:[r.getBoundsX(),r.getBoundsY(),r.getBoundsWidth(),r.getBoundsHeight()],area:r.getArea(),
         plane:[r.getC(),r.getZ(),r.getT()],case_id:o.isTMACore()?(o.getCaseID()?:''):null,missing:o.isTMACore()?o.isMissing():null,
         metadata:new TreeMap(o.getMetadata()),measurements:o.getMeasurementList().asMap(),classification:o.getPathClass()?.toString(),locked:o.isLocked(),color:o.getColor(),nucleus_area:nucleus?.getArea()]
    }
    summaries.add([name:label,columns:grid?.getGridWidth(),rows:grid?.getGridHeight(),core_ids:grid?.getTMACoreList()?.collect{it.getID().toString()},objects:restored.getHierarchy().getAllObjects(false).collect{describe(it)}])
    restored.getServer().close();server.close()
}
project.syncChanges();Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(summaries)+'\n')
