// Fixed public-API workflow; oracle rasterization is outside workflow timers.
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServers
import qupath.lib.regions.RegionRequest
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs
import qupath.lib.objects.PathObjects
import qupath.lib.scripting.QP
import qupath.lib.color.ColorDeconvolutionStains
import qupath.lib.color.StainVector
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools
import qupath.lib.common.GeneralTools
import java.lang.management.ManagementFactory
import java.nio.file.Files
import java.nio.file.Path
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest

def source=Path.of(args[0]);def output=Path.of(args[1])
boolean resourceOnly=args.length>2 && args[2]=='resource'
boolean steady=args.length>2 && args[2]=='steady'
boolean skipOracles=resourceOnly || steady
int warmupRuns=steady ? (args.length>3 ? Integer.parseInt(args[3]) : 6) : resourceOnly ? 0 : 1
int measuredRuns=steady ? (args.length>4 ? Integer.parseInt(args[4]) : 6) : resourceOnly ? 1 : 3
if(warmupRuns<0 || warmupRuns>100 || measuredRuns<1 || measuredRuns>100)
    throw new IllegalArgumentException('Invalid warmup/measured workflow count')
Files.createDirectory(output)
Files.writeString(output.resolve('pid.txt'),Long.toString(ProcessHandle.current().pid()))
def gson=GsonTools.getInstance();def runs=[]
def cpuTime={ProcessHandle.current().info().totalCpuDuration().orElse(java.time.Duration.ZERO).toNanos()}
def bounds=[11392,17792,256,256]
def progress=Files.newBufferedWriter(output.resolve('progress.jsonl'))
for(int iteration=0;iteration<warmupRuns+measuredRuns;iteration++) {
    def folder=output.resolve(Integer.toString(iteration));if(iteration==warmupRuns+measuredRuns-1)Files.createDirectory(folder)
    def stages=[:]
    def timed={String name,Closure operation ->
        long wall=System.nanoTime(),cpu=cpuTime()
        def value=operation()
        stages[name]=[wall_ms:(System.nanoTime()-wall)/1e6,cpu_ms:(cpuTime()-cpu)/1e6]
        progress.write(gson.toJson([iteration:iteration,stage:name,wall_ms:stages[name].wall_ms]));progress.newLine();progress.flush()
        value
    }
    long start=System.nanoTime()
    def data=timed('open_image') {
        def server=ImageServers.buildServer(source.toUri(),'--classname','qupath.lib.images.servers.openslide.OpenslideServerBuilder')
        new ImageData(server,ImageData.ImageType.BRIGHTFIELD_H_E)
    }
    def server=data.getServer()
    try {
        double px=server.getPixelCalibration().getPixelWidthMicrons(),py=server.getPixelCalibration().getPixelHeightMicrons()
        double average=(px+py)/2d
        def roi=ROIs.createRectangleROI(*bounds.collect {it as double},ImagePlane.getDefaultPlane())
        def annotation=timed('setup_roi') {
            def object=PathObjects.createAnnotationObject(roi)
            data.getHierarchy().addObject(object);data.getHierarchy().getSelectionModel().setSelectedObject(object)
            data.setColorDeconvolutionStains(new ColorDeconvolutionStains('Explicit two-vector model',
                StainVector.createStainVector('A',.65,.70,.29),StainVector.createStainVector('B',.07,.99,.11),255d,255d,255d))
            object
        }
        def image=timed('read_roi') {server.readRegion(RegionRequest.createInstance(server.getPath(),1d,*bounds))}
        int width=bounds[2],height=bounds[3]
        byte[] rgb=new byte[width*height*3];long[] sums=new long[3]
        // This scripted RGB reduction is not the native OD+RGB statistics API;
        // report it separately, never claim a paired statistics-kernel speedup.
        timed('scripted_rgb_statistics') {
            int[] packed=image.getRGB(0,0,width,height,null,0,width)
            for(int i=0;i<packed.length;i++) {
                int value=packed[i]
                int[] channels=[(value>>>16)&255,(value>>>8)&255,value&255]
                for(int c=0;c<3;c++) {rgb[3*i+c]=(byte)channels[c];sums[c]+=channels[c]}
            }
        }
        def parameters=[detectionImageBrightfield:'Optical density sum',requestedPixelSizeMicrons:average,
            backgroundRadiusMicrons:0d,backgroundByReconstruction:true,medianRadiusMicrons:0d,
            sigmaMicrons:average,minAreaMicrons:16*average*average,maxAreaMicrons:512*average*average,
            threshold:.4d,maxBackground:2d,watershedPostProcess:true,excludeDAB:false,
            cellExpansionMicrons:5*average,includeNuclei:true,smoothBoundaries:false,makeMeasurements:true]
        timed('cell_detection_and_publication') {
            if(!QP.runPlugin('qupath.imagej.detect.cells.WatershedCellDetection',data,parameters))
                throw new IllegalStateException('Cell plugin failed')
        }
        def cells=data.getHierarchy().getCellObjects().toList()
        def density=timed('density') {
            def selected=cells.count {roi.contains(it.getNucleusROI().getCentroidX(),it.getNucleusROI().getCentroidY())}
            double area=width*height*px*py/1e6
            annotation.getMeasurementList().put('ROI Cell count',selected as double)
            annotation.getMeasurementList().put('ROI Cells per mm2',selected/area)
            [count:selected,area_mm2:area,density:selected/area]
        }
        if(iteration!=warmupRuns+measuredRuns-1) {
            runs.add([iteration:iteration,warmup:iteration<warmupRuns,stages:stages,cells:cells.size(),
                area_mm2:density.area_mm2,density:density.density,rgb_sum:sums.toList(),pixel_width_um:px,pixel_height_um:py])
            continue
        }
        timed('export_csv') {Files.writeString(folder.resolve('density.csv'),
            'count,area_mm2,density_per_mm2\n'+density.count+','+density.area_mm2+','+density.density+'\n')}
        timed('save_image_data') {PathIO.writeImageData(folder.resolve('study.qpdata'),data)}
        def reopened=timed('reopen_image_data') {
            def restored=PathIO.readImageData(folder.resolve('study.qpdata'),server.getBuilder())
            restored.getServer();restored
        }
        double workflowMs=(System.nanoTime()-start)/1e6
        if(reopened.getHierarchy().getCellObjects().size()!=cells.size()) throw new IllegalStateException('Reopened cell count differs')
        reopened.getServer().close()
        // Independent pixel-center partition capture, outside all timers.
        ['nucleus','body'].each {compartment ->
            int[] labels=new int[width*height]
            cells.eachWithIndex {cell,index ->
                def shape=compartment=='nucleus'?cell.getNucleusROI():cell.getROI()
                int left=Math.max(0,(int)Math.floor(shape.getBoundsX())-bounds[0])
                int top=Math.max(0,(int)Math.floor(shape.getBoundsY())-bounds[1])
                int right=Math.min(width,(int)Math.ceil(shape.getBoundsX()+shape.getBoundsWidth())-bounds[0])
                int bottom=Math.min(height,(int)Math.ceil(shape.getBoundsY()+shape.getBoundsHeight())-bounds[1])
                for(int y=top;y<bottom;y++) for(int x=left;x<right;x++)
                    if(shape.contains(bounds[0]+x+.5,bounds[1]+y+.5)) {
                        int at=y*width+x
                        if(labels[at]!=0) throw new IllegalStateException('Overlapping cell partition')
                        labels[at]=index+1
                    }
            }
            def bytes=ByteBuffer.allocate(labels.length*4).order(ByteOrder.LITTLE_ENDIAN)
            labels.each {bytes.putInt(it)};Files.write(folder.resolve(compartment+'.u32'),bytes.array())
        }
        Files.write(folder.resolve('rgb.u8'),rgb)
        runs.add([iteration:iteration,warmup:iteration<warmupRuns,stages:stages,workflow_ms:workflowMs,
            cells:cells.size(),density:density.density,area_mm2:density.area_mm2,
            rgb_sha256:MessageDigest.getInstance('SHA-256').digest(rgb).encodeHex().toString(),rgb_sum:sums.toList(),
            pixel_width_um:px,pixel_height_um:py,parameters:parameters,
            project_bytes:Files.size(folder.resolve('study.qpdata'))])
        println(gson.toJson([iteration:iteration,cells:cells.size(),workflow_ms:workflowMs]))
    } finally {server.close()}
}
progress.close()
Files.writeString(output.resolve('report.json'),gson.toJson([engine:'QuPath',version:GeneralTools.getVersion(),
    java_version:System.getProperty('java.version'),roi:bounds,warmup_runs:warmupRuns,measured_runs:measuredRuns,runs:runs]))
