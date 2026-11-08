// Capture through public QuPath APIs; the native implementation is independent.
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
import javax.imageio.ImageIO
import java.nio.file.Files
import java.nio.file.Path
import java.nio.charset.StandardCharsets
import java.util.zip.GZIPOutputStream

def root=Path.of(args[0])
def contract=GsonTools.getInstance().fromJson(Files.readString(root.resolve('contract.json')),Map.class)
def server=ImageServers.buildServer(Path.of(args[1]).toUri(),
    '--classname','qupath.lib.images.servers.openslide.OpenslideServerBuilder')
try {
    def calibration=server.getPixelCalibration()
    double px=calibration.getPixelWidthMicrons(),py=calibration.getPixelHeightMicrons()
    if(!Double.isFinite(px) || !Double.isFinite(py)) throw new IllegalStateException('Missing native calibration')
    def bounds=contract.roi.collect {it as int}
    def image=server.readRegion(RegionRequest.createInstance(server.getPath(),1d,*bounds))
    ImageIO.write(image,'PNG',root.resolve('input.png').toFile())
    def data=new ImageData(server,ImageData.ImageType.BRIGHTFIELD_H_E)
    def stains=new ColorDeconvolutionStains('Explicit two-vector model',
        StainVector.createStainVector('A',*contract.vectors[0]),
        StainVector.createStainVector('B',*contract.vectors[1]),*contract.white.collect {it as double})
    data.setColorDeconvolutionStains(stains)
    def roi=ROIs.createRectangleROI(*bounds.collect {it as double},ImagePlane.getDefaultPlane())
    def annotation=PathObjects.createAnnotationObject(roi)
    data.getHierarchy().addObject(annotation)
    data.getHierarchy().getSelectionModel().setSelectedObject(annotation)
    double average=(px+py)/2d
    def nativeParameters=contract.native
    def params=[detectionImageBrightfield:'Optical density sum',requestedPixelSizeMicrons:average,
        backgroundRadiusMicrons:0d,backgroundByReconstruction:true,medianRadiusMicrons:0d,
        sigmaMicrons:nativeParameters.sigma*average,
        minAreaMicrons:nativeParameters.min_nucleus_pixels*average*average,
        maxAreaMicrons:nativeParameters.max_nucleus_pixels*average*average,
        threshold:nativeParameters.threshold,maxBackground:2d,watershedPostProcess:true,
        excludeDAB:false,cellExpansionMicrons:nativeParameters.cell_expansion*average,
        includeNuclei:true,smoothBoundaries:false,makeMeasurements:true]
    if(!QP.runPlugin('qupath.imagej.detect.cells.WatershedCellDetection',data,params))
        throw new IllegalStateException('NDPI cell plugin failed')
    def cells=data.getHierarchy().getCellObjects().toList().sort {a,b ->
        (a.nucleusROI.centroidY <=> b.nucleusROI.centroidY) ?:
        (a.nucleusROI.centroidX <=> b.nucleusROI.centroidX) ?:
        (a.nucleusROI.area <=> b.nucleusROI.area)
    }
    cells.eachWithIndex {cell,i -> cell.setName('cell_'+(i+1))}
    PathIO.exportObjectsAsGeoJSON(root.resolve('cells.geojson'),cells,
        PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,
        PathIO.GeoJsonExportOptions.EXCLUDE_MEASUREMENTS,
        PathIO.GeoJsonExportOptions.PRETTY_JSON)
    int width=bounds[2],height=bounds[3]
    def counts=[:],overlaps=[:]
    ['nucleus','body'].each {compartment ->
        def labels=new int[width*height]
        def sampled=[]
        int collisions=0
        cells.eachWithIndex {cell,i ->
            def shape=compartment=='nucleus'?cell.nucleusROI:cell.ROI
            int count=0
            for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
                if(shape.contains(bounds[0]+x+.5,bounds[1]+y+.5)) {
                    int index=y*width+x
                    if(labels[index]!=0) collisions++
                    else labels[index]=i+1
                    count++
                }
            }
            sampled.add(count)
        }
        counts[compartment]=sampled
        overlaps[compartment]=collisions
        new GZIPOutputStream(Files.newOutputStream(root.resolve(compartment+'.u32le.gz'))).withCloseable {out ->
            labels.each {label -> for(int shift=0;shift<32;shift+=8) out.write((label>>>shift)&255)}
        }
    }
    def report=[schema:'qpx.qupath-ndpi-cells-reference/1',qupath_version:GeneralTools.getVersion(),
        java_version:System.getProperty('java.version'),server_class:server.getClass().getName(),
        server_type:server.getServerType(),source_width:server.getWidth(),source_height:server.getHeight(),
        pixel_size_um:[px,py],roi:bounds,requested_downsample:1d,parameters:params,
        count:cells.size(),sampled_pixels:counts,collisions:overlaps,
        plugin_measurements:cells.collect {cell -> [name:cell.getName(),measurements:cell.getMeasurementList().asMap()]}]
    Files.writeString(root.resolve('reference.json'),GsonTools.getInstance(true).toJson(report)+'\n',StandardCharsets.UTF_8)
    println('QPX_REAL_NDPI_CELLS_OK '+cells.size())
} finally {server.close()}
