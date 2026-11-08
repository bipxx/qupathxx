// One untimed probe: observe the plugin's real RegionRequest, without replacing pixels.
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServers
import qupath.lib.images.servers.ImageServer
import qupath.lib.roi.ROIs
import qupath.lib.regions.ImagePlane
import qupath.lib.objects.PathObjects
import qupath.lib.scripting.QP
import qupath.lib.color.ColorDeconvolutionStains
import qupath.lib.color.StainVector
import qupath.lib.io.GsonTools
import java.nio.file.Files
import java.nio.file.Path
import java.lang.reflect.Proxy
import java.lang.reflect.InvocationHandler
def source=Path.of(args[0]),output=Path.of(args[1]),gson=GsonTools.getInstance()
def reference=gson.fromJson(Files.readString(output.resolve('qupath-retry/report.json')),Map.class)
def real=ImageServers.buildServer(source.toUri(),'--classname','qupath.lib.images.servers.openslide.OpenslideServerBuilder')
def requests=[]
def handler={proxy,method,arguments ->
    if(method.name=='readRegion') {
        def r=arguments[0]
        requests.add([x:r.getX(),y:r.getY(),width:r.getWidth(),height:r.getHeight(),downsample:r.getDownsample()])
    }
    method.invoke(real,arguments)
} as InvocationHandler
def server=Proxy.newProxyInstance(ImageServer.class.classLoader,[ImageServer] as Class[],handler) as ImageServer
try {
    def data=new ImageData(server,ImageData.ImageType.BRIGHTFIELD_H_E)
    data.setColorDeconvolutionStains(new ColorDeconvolutionStains('Explicit two-vector model',
        StainVector.createStainVector('A',.65,.70,.29),StainVector.createStainVector('B',.07,.99,.11),255d,255d,255d))
    def annotation=PathObjects.createAnnotationObject(ROIs.createRectangleROI(29185d,2048d,512d,512d,ImagePlane.getDefaultPlane()))
    data.getHierarchy().addObject(annotation);data.getHierarchy().getSelectionModel().setSelectedObject(annotation)
    if(!QP.runPlugin('qupath.imagej.detect.cells.WatershedCellDetection',data,reference.runs[1].parameters)) throw new IllegalStateException('Probe failed')
    Files.writeString(output.resolve('qupath-requests.json'),gson.toJson([requests:requests,cells:data.getHierarchy().getCellObjects().size()]))
} finally {real.close()}
