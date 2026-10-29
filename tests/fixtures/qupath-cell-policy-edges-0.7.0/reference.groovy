// Independent public-API capture; no upstream segmentation implementation is copied.
import qupath.lib.images.ImageData
import qupath.lib.images.servers.WrappedBufferedImageServer
import qupath.lib.images.servers.ImageServerMetadata
import qupath.lib.roi.ROIs
import qupath.lib.roi.GeometryTools
import qupath.lib.regions.ImagePlane
import qupath.lib.objects.PathObjects
import qupath.lib.scripting.QP
import qupath.imagej.detect.cells.WatershedCellDetection
import qupath.lib.color.ColorTransformer
import qupath.lib.color.ColorDeconvolutionStains
import qupath.lib.color.StainVector
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools
import qupath.lib.common.GeneralTools
import java.awt.image.BufferedImage
import javax.imageio.ImageIO
import java.nio.file.Files
import java.nio.file.Path
import java.nio.charset.StandardCharsets
import java.util.zip.GZIPOutputStream

def root=Path.of(args[0])
def contract=GsonTools.getInstance().fromJson(Files.readString(root.resolve('contract.json')),Map.class)
def plane=ImagePlane.getDefaultPlane()
def stains=new ColorDeconvolutionStains('Explicit two-vector model',
    StainVector.createStainVector('A',*contract.vectors[0]),
    StainVector.createStainVector('B',*contract.vectors[1]),*contract.white.collect {it as double})
WatershedCellDetection.setDebugMode(true)
contract.cases.eachWithIndex { item, caseIndex ->
    def folder=root.resolve(item.name)
    captureFolder=folder
    def recipe=item.native ?: contract.native
    def input=ImageIO.read(folder.resolve('input.png').toFile())
    def image=new BufferedImage(input.width,input.height,BufferedImage.TYPE_INT_RGB)
    def graphics=image.createGraphics();graphics.drawImage(input,0,0,null);graphics.dispose()
    def server=new WrappedBufferedImageServer(item.name,image)
    try {
        def data=new ImageData(server,ImageData.ImageType.BRIGHTFIELD_H_E)
        data.updateServerMetadata(new ImageServerMetadata.Builder(server.getMetadata())
            .pixelSizeMicrons(*item.pixel_size_um.collect {it as double}).build())
        data.setColorDeconvolutionStains(stains)
        def bounds=item.roi ?: [0,0,image.width,image.height]
        def roi=ROIs.createRectangleROI(*bounds.collect {it as double},plane)
        if(item.hole) {
            def hole=ROIs.createRectangleROI(*item.hole.collect {it as double},plane)
            roi=GeometryTools.geometryToROI(roi.getGeometry().difference(hole.getGeometry()),plane)
        }
        def annotation=PathObjects.createAnnotationObject(roi)
        data.getHierarchy().addObject(annotation)
        data.getHierarchy().getSelectionModel().setSelectedObject(annotation)
        double pixelSize=(item.pixel_size_um[0]+item.pixel_size_um[1])/2d
        def params=[detectionImageBrightfield:'Optical density sum',
            requestedPixelSizeMicrons:(item.requested_pixel_size_um ?: pixelSize),
            backgroundRadiusMicrons:0d,backgroundByReconstruction:true,
            medianRadiusMicrons:0d,sigmaMicrons:recipe.sigma*pixelSize,
            minAreaMicrons:recipe.min_nucleus_pixels*pixelSize*pixelSize,
            maxAreaMicrons:recipe.max_nucleus_pixels*pixelSize*pixelSize,
            threshold:recipe.threshold,maxBackground:2d,
            watershedPostProcess:true,excludeDAB:false,
            cellExpansionMicrons:recipe.cell_expansion*pixelSize,
            includeNuclei:true,smoothBoundaries:false,makeMeasurements:true]
        double probeDownsample=params.requestedPixelSizeMicrons/pixelSize
        def probeRequest=qupath.lib.regions.RegionRequest.createInstance(server.getPath(),probeDownsample,
            *(bounds.collect {it as int}))
        def probeImage=server.readRegion(probeRequest)
        if(!ImageIO.write(probeImage,'PNG',folder.resolve('processed-input.png').toFile()))
            throw new IllegalStateException('Cannot save processed input')
        Files.writeString(folder.resolve('processed-input.json'),GsonTools.getInstance(true).toJson(
            [downsample:probeDownsample,request:bounds,width:probeImage.width,height:probeImage.height,
             bufferedImageType:probeImage.type])+'\n')
        if(!QP.runPlugin('qupath.imagej.detect.cells.WatershedCellDetection',data,params))
            throw new IllegalStateException('Cell plugin failed: '+item.name)
        def cells=data.getHierarchy().getCellObjects().toList().sort {a,b ->
            (a.nucleusROI.centroidY <=> b.nucleusROI.centroidY) ?:
            (a.nucleusROI.centroidX <=> b.nucleusROI.centroidX) ?:
            (a.nucleusROI.area <=> b.nucleusROI.area)
        }
        cells.eachWithIndex {cell,i ->
            cell.setName('cell_'+(i+1))
            cell.setID(UUID.fromString(String.format('00000000-0000-0000-0000-%012d',caseIndex*1000+i+1)))
        }
        PathIO.exportObjectsAsGeoJSON(folder.resolve('cells.geojson'),cells,
            PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,
            PathIO.GeoJsonExportOptions.EXCLUDE_MEASUREMENTS,
            PathIO.GeoJsonExportOptions.PRETTY_JSON)
        int width=image.width,height=image.height
        def rgb=image.getRGB(0,0,width,height,null,0,width)
        def methods=[ColorTransformer.ColorTransformMethod.Red_OD,
            ColorTransformer.ColorTransformMethod.Green_OD,ColorTransformer.ColorTransformMethod.Blue_OD,
            ColorTransformer.ColorTransformMethod.Stain_1,ColorTransformer.ColorTransformMethod.Stain_2,
            ColorTransformer.ColorTransformMethod.Stain_3]
        def channels=methods.collect {ColorTransformer.getTransformedPixels(rgb,it,null as float[],stains)}
        def records=[];def collisions=[:];def decodedRecords=[];def decodedCollisions=[:]
        def decoded=PathIO.readObjects(folder.resolve('cells.geojson')).toList()
        [[objects:cells,suffix:'',regions:records,collisions:collisions],
         [objects:decoded,suffix:'-decoded',regions:decodedRecords,collisions:decodedCollisions]].each {sampling ->
        ['nucleus','body'].each {compartment ->
            def labels=new int[width*height];int overlaps=0
            sampling.objects.eachWithIndex {cell,i ->
                def shape=compartment=='nucleus'?cell.nucleusROI:cell.ROI
                long count=0;def sums=new double[9]
                for(int y=0;y<height;y++) for(int x=0;x<width;x++) if(shape.contains(x+.5,y+.5)) {
                    int index=y*width+x
                    if(labels[index]!=0) overlaps++
                    else labels[index]=i+1
                    count++
                    sums[0]+=(rgb[index]>>16)&255;sums[1]+=(rgb[index]>>8)&255;sums[2]+=rgb[index]&255
                    for(int c=0;c<6;c++) sums[c+3]+=channels[c][index]
                }
                sampling.regions.add([label:i+1,compartment:compartment,pixels:count,
                    rgb_mean:(0..2).collect {count?sums[it]/count:null},
                    od_mean:(3..5).collect {count?sums[it]/count:null},
                    stain_mean:(6..8).collect {count?sums[it]/count:null},
                    area:shape.area,area_um2:shape.getScaledArea(*item.pixel_size_um.collect {it as double}),
                    centroid:[shape.centroidX,shape.centroidY]])
            }
            sampling.collisions[compartment]=overlaps
            new GZIPOutputStream(Files.newOutputStream(folder.resolve(compartment+sampling.suffix+'.u32le.gz'))).withCloseable {out ->
                labels.each {label -> for(int shift=0;shift<32;shift+=8) out.write((label>>>shift)&255)}
            }
        }
        }
        def pluginMeasurements=cells.collect {cell ->
            [label:cells.indexOf(cell)+1,measurements:cell.getMeasurementList().asMap()]
        }
        def result=[schema:'qpx.qupath-cells-reference/1',case_name:item.name,
            qupath_version:GeneralTools.getVersion(),java_version:System.getProperty('java.version'),
            width:width,height:height,parameters:params,count:cells.size(),
            collisions:collisions,regions:records,decoded_regions:decodedRecords,
            decoded_collisions:decodedCollisions,plugin_measurements:pluginMeasurements]
        Files.writeString(folder.resolve('reference.json'),GsonTools.getInstance(true).toJson(result)+'\n',StandardCharsets.UTF_8)
        println('QPX_CELLS_OK '+item.name+' '+cells.size())
    } finally {server.close()}
}
