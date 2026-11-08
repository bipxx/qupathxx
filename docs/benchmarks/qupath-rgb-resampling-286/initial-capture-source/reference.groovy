// Invoke only public QuPath APIs. No reader or interpolation implementation is copied.
import qupath.lib.images.servers.WrappedBufferedImageServer
import qupath.lib.regions.RegionRequest
import qupath.lib.io.GsonTools
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import javax.imageio.ImageIO

def root=Path.of(args[0])
def contract=GsonTools.getInstance().fromJson(Files.readString(root.resolve('contract.json')),Map.class)
def results=[]
contract.cases.each { item ->
    def folder=root.resolve(item.name)
    def input=ImageIO.read(folder.resolve('input.png').toFile())
    def rgb=new BufferedImage(input.width,input.height,BufferedImage.TYPE_INT_RGB)
    def graphics=rgb.createGraphics()
    graphics.drawImage(input,0,0,null)
    graphics.dispose()
    def server=new WrappedBufferedImageServer(item.name,rgb)
    try {
        item.requests.each { request ->
            def bounds=request.bounds.collect {it as int}
            def region=RegionRequest.createInstance(server.getPath(),request.downsample as double,*bounds)
            def output=server.readRegion(region)
            def file=folder.resolve(request.name+'.png')
            if(!ImageIO.write(output,'PNG',file.toFile())) throw new IllegalStateException('Cannot save capture')
            results.add([case_name:item.name,request_name:request.name,downsample:request.downsample,
                bounds:bounds,width:output.width,height:output.height,buffered_image_type:output.type,
                server_class:server.getClass().getName()])
        }
    } finally {server.close()}
}
Files.writeString(root.resolve('reference.json'),GsonTools.getInstance(true).toJson(
    [schema:'qpx.qupath-rgb-resampling-capture/1',qupath_version:getQuPathVersion(),
     java_version:System.getProperty('java.version'),cases:results])+'\n')
