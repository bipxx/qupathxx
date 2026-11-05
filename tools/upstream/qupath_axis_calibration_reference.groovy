// Pinned QuPath0.7 saved physical Z/time calibration; one output directory.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import java.util.concurrent.TimeUnit
import qupath.lib.projects.Projects
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.images.servers.ImageServerMetadata
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools
import qupath.lib.objects.PathObjects
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs

def output=Path.of(args[0]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class);def rows=[];int index=0
def reused=null
if(args.length>1)reused=Path.of(args[1])
def cases=[
    [name:'nanoseconds',unit:TimeUnit.NANOSECONDS,times:[0,250000000,500000000,750000000],z:4.5],
    [name:'microseconds',unit:TimeUnit.MICROSECONDS,times:[0,250000,500000,750000],z:4.5],
    [name:'milliseconds-offset',unit:TimeUnit.MILLISECONDS,times:[1250,1500,1750,2000],z:4.5],
    [name:'seconds',unit:TimeUnit.SECONDS,times:[2,3,4,5],z:4.5],
    [name:'minutes-irregular',unit:TimeUnit.MINUTES,times:[-0.5,0,0.75,2],z:4.5],
    [name:'hours',unit:TimeUnit.HOURS,times:[0,0.5,1,2],z:4.5],
    [name:'days',unit:TimeUnit.DAYS,times:[0,1,2,3],z:4.5],
    [name:'partial',unit:TimeUnit.SECONDS,times:[-0.0,3.25],z:4.5],
    [name:'nonmonotonic',unit:TimeUnit.SECONDS,times:[5,3,3,-1],z:4.5],
    [name:'unknown',unit:TimeUnit.SECONDS,times:[],z:Double.NaN]
]
for(def spec:cases) {
    def server=ImageServerProvider.buildServer(output.resolve('source.ome.tif').toString(),BufferedImage.class)
    def entry=project.addImage(server.getBuilder());entry.setImageName(spec.name)
    def data=new ImageData(server)
    index++
    boolean reuse=reused!=null && index<=7
    if(reuse) {
        def destination=directory.resolve('data').resolve(entry.getID()).resolve('data.qpdata')
        Files.createDirectories(destination.getParent())
        Files.copy(reused.resolve('data').resolve(index.toString()).resolve('data.qpdata'),destination)
    }
    if(!reuse) {
    data.updateServerMetadata(new ImageServerMetadata.Builder(server.getMetadata()).zSpacingMicrons(spec.z)
        .timepoints(spec.unit,spec.times.collect{it as double} as double[]).build())
    def roi=PathObjects.createAnnotationObject(ROIs.createRectangleROI(1,2,8,9,ImagePlane.getPlane(2,3)))
    roi.setName('Last plane');data.getHierarchy().getRootObject().addChildObject(roi)
    entry.saveImageData(data)
    }
    def restored=PathIO.readImageData(directory.resolve('data').resolve(entry.getID()).resolve('data.qpdata'),(qupath.lib.images.servers.ImageServerBuilder.ServerBuilder)null);def metadata=restored.getServerMetadata();def cal=metadata.getPixelCalibration()
    def secondsPerUnit=[NANOSECONDS:1e-9,MICROSECONDS:1e-6,MILLISECONDS:1e-3,SECONDS:1.0,MINUTES:60.0,HOURS:3600.0,DAYS:86400.0][cal.getTimeUnit().name()]
    rows.add([reused:reuse,name:spec.name,z_spacing_um:Double.toHexString(cal.getZSpacingMicrons()),time_unit:cal.getTimeUnit().name(),
        times:(0..<cal.nTimepoints()).collect{Double.toHexString(cal.getTimepoint(it))},
        seconds:(0..<cal.nTimepoints()).collect{Double.toHexString(cal.getTimepoint(it)*secondsPerUnit)},
        z_planes:metadata.getSizeZ(),timepoints:metadata.getSizeT(),plane:[2,3]])
    restored.getServer().close();server.close()
    project.syncChanges()
    Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(rows)+'\n')
}
project.syncChanges()
Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(rows)+'\n')
