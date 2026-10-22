// Independent QuPath v0.7.0 API reference generator. See docs/qupath-comparisons.md.
// Output directory is args[0]; optional args[1] is native GeoJSON to round-trip.
import qupath.lib.roi.ROIs
import qupath.lib.roi.GeometryTools
import qupath.lib.regions.ImagePlane
import qupath.lib.objects.PathObjects
import qupath.lib.objects.classes.PathClass
import qupath.lib.common.ColorTools
import qupath.lib.common.GeneralTools
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools
import qupath.lib.color.ColorTransformer
import qupath.lib.color.ColorDeconvolutionStains
import qupath.lib.color.StainVector
import org.locationtech.jts.io.WKTReader
import java.nio.file.Files
import java.nio.file.Path
import java.nio.charset.StandardCharsets

def root=Path.of(args[0]); Files.createDirectories(root)
def plane=ImagePlane.getDefaultPlane()
def cls=PathClass.getInstance('Tumor',ColorTools.packRGB(255,32,48))
def wkt=new WKTReader()
def objects=[
 PathObjects.createAnnotationObject(ROIs.createRectangleROI(10.25,20.5,40.5,25.25,plane),cls),
 PathObjects.createAnnotationObject(ROIs.createEllipseROI(70,10,30,20,plane),cls),
 PathObjects.createAnnotationObject(GeometryTools.geometryToROI(wkt.read('POLYGON ((10 80,60 80,60 120,10 120,10 80),(20 90,20 100,35 100,35 90,20 90))'),plane),cls),
 PathObjects.createAnnotationObject(GeometryTools.geometryToROI(wkt.read('MULTIPOLYGON (((80 80,95 80,95 100,80 100,80 80)),((110 80,130 80,130 100,110 100,110 80)))'),plane)),
 PathObjects.createAnnotationObject(ROIs.createPolylineROI([15d,30d,40d] as double[],[150d,160d,145d] as double[],plane)),
 PathObjects.createAnnotationObject(ROIs.createPointsROI([80d,90d,100d] as double[],[150d,165d,145d] as double[],plane)),
 PathObjects.createDetectionObject(ROIs.createRectangleROI(150,20,10,15,plane),cls),
 PathObjects.createCellObject(ROIs.createRectangleROI(170,70,40,40,plane),ROIs.createEllipseROI(180,80,15,10,plane),cls,null),
 PathObjects.createAnnotationObject(ROIs.createRectangleROI(150.499,150.499,20.002,20.002,plane),cls),
 PathObjects.createAnnotationObject(ROIs.createEllipseROI(20,219,200,2,plane),cls)
]
objects.eachWithIndex {obj,i -> obj.setName(['Rectangle','Ellipse','Hole','Multipart','Polyline','Points \u03bc','Detection','Cell','Subpixel boundary','Eccentric ellipse'][i]);obj.setID(UUID.fromString(String.format('00000000-0000-0000-0000-%012d',i+1))) }
if(args.length>1) objects=PathIO.readObjects(Path.of(args[1]))
PathIO.exportObjectsAsGeoJSON(root.resolve('objects.geojson'),objects,PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,PathIO.GeoJsonExportOptions.EXCLUDE_MEASUREMENTS,PathIO.GeoJsonExportOptions.PRETTY_JSON)
def geometry={r -> [area:r.getArea(),length:r.getLength(),area_um2:r.getScaledArea(.5,.25),length_um:r.getScaledLength(.5,.25),bounds:[r.getBoundsX(),r.getBoundsY(),r.getBoundsWidth(),r.getBoundsHeight()]]}
def created=objects
def decoded=PathIO.readObjects(root.resolve('objects.geojson'))
objects=decoded
def measurements=objects.collect {o -> [id:o.getID().toString(),name:o.getName(),kind:o.isCell()?'cell':o.isDetection()?'detection':'annotation',classification:o.getPathClass()?.toString(),class_color:o.getPathClass()?.getColor(),geometry:geometry(o.getROI()),nucleus:o.isCell()?geometry(o.getNucleusROI()):null]}
def data=[schema:'qpx.qupath-reference/1',qupath_version:GeneralTools.getVersion(),java_version:System.getProperty('java.version'),calibration:[.5,.25],objects:measurements,created_geometry:created.collect {o -> [name:o.getName(),geometry:geometry(o.getROI())]}]
Files.writeString(root.resolve('measurements.json'),GsonTools.getInstance(true).toJson(data)+'\n',StandardCharsets.UTF_8)
def pixels=[[0,0,0],[255,255,255],[50,20,190],[1,254,128]]
(0..255).each {i -> pixels.add([i,(i*37)%256,255-i])}
def vectors=[[.65,.70,.29],[.07,.99,.11]]
def models=[[255d,255d,255d],[190d,210d,230d]].collect {white ->
 def stains=new ColorDeconvolutionStains('Explicit two-vector model',StainVector.createStainVector('A',*vectors[0]),StainVector.createStainVector('B',*vectors[1]),*white)
 def rgb=pixels.collect {p -> ColorTools.packRGB(*p)} as int[]
 def methods=[ColorTransformer.ColorTransformMethod.Red_OD,ColorTransformer.ColorTransformMethod.Green_OD,ColorTransformer.ColorTransformMethod.Blue_OD,ColorTransformer.ColorTransformMethod.Stain_1,ColorTransformer.ColorTransformMethod.Stain_2,ColorTransformer.ColorTransformMethod.Stain_3]
 [white:white,basis:(1..3).collect {stains.getStain(it).getArray()},channels:methods.collect {ColorTransformer.getTransformedPixels(rgb,it,null as float[],stains)}]
}
Files.writeString(root.resolve('colors.json'),GsonTools.getInstance(true).toJson([schema:'qpx.qupath-colors/1',pixels:pixels,vectors:vectors,models:models])+'\n',StandardCharsets.UTF_8)

// Sample at explicit level-zero pixel centers, independently of native masks.
def width=256, height=256
def raster=new int[width*height]
for(int y=0;y<height;y++) for(int x=0;x<width;x++) raster[y*width+x]=ColorTools.packRGB((x*3+y*5)%256,(x*7+y*11)%256,(x*13+y*17)%256)
def model=new ColorDeconvolutionStains('Explicit two-vector model',StainVector.createStainVector('A',*vectors[0]),StainVector.createStainVector('B',*vectors[1]),255d,255d,255d)
def transforms=[ColorTransformer.ColorTransformMethod.Red_OD,ColorTransformer.ColorTransformMethod.Green_OD,ColorTransformer.ColorTransformMethod.Blue_OD,ColorTransformer.ColorTransformMethod.Stain_1,ColorTransformer.ColorTransformMethod.Stain_2,ColorTransformer.ColorTransformMethod.Stain_3].collect {ColorTransformer.getTransformedPixels(raster,it,null as float[],model)}
def sampleObjects={items ->
 def samples=[]
 items.each {obj ->
 def compartments=[body:obj.getROI()]
 if(obj.isCell()) compartments.nucleus=obj.getNucleusROI()
 compartments.each {name,roi ->
  if(roi.isArea()) {
   long count=0;def sums=new double[6];def mask=new byte[width*height]
   for(int y=0;y<height;y++) for(int x=0;x<width;x++) if(roi.contains(x+.5,y+.5)) {
    mask[y*width+x]=1;count++; for(int c=0;c<6;c++) sums[c]+=transforms[c][y*width+x]
   }
   samples.add([name:obj.getName(),compartment:name,samples:count,mask_sha256:HexFormat.of().formatHex(java.security.MessageDigest.getInstance('SHA-256').digest(mask)),od_means:(0..2).collect {sums[it]/count},stain_means:(3..5).collect {sums[it]/count}])
  }
 }
}
 return samples
}
def samples=sampleObjects(objects)
Files.writeString(root.resolve('roi-means.json'),GsonTools.getInstance(true).toJson([schema:'qpx.qupath-roi-means/1',width:width,height:height,pattern:'r=(3*x+5*y)%256;g=(7*x+11*y)%256;b=(13*x+17*y)%256',white:[255,255,255],vectors:vectors,regions:samples,created_regions:sampleObjects(created)])+'\n',StandardCharsets.UTF_8)


// Retain edge cases even when numerical parity is not achieved.
def edgeObjects=[
 PathObjects.createAnnotationObject(ROIs.createEllipseROI(170,220.45,60,.1,plane),cls),
 PathObjects.createAnnotationObject(GeometryTools.geometryToROI(wkt.read('POLYGON ((10 80,60 80,60 120,10 120,10 80),(20.5 90.5,20.5 100.5,35.5 100.5,35.5 90.5,20.5 90.5))'),plane),cls)
]
def edgeCases=edgeObjects.withIndex().collect {edge,i ->
 edge.setName(['Very thin ellipse','Half-pixel hole'][i])
 edge.setID(UUID.fromString(String.format('00000000-0000-0000-0000-%012d',11+i)))
 def stream=new ByteArrayOutputStream()
 PathIO.exportObjectsAsGeoJSON(stream,[edge],PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,PathIO.GeoJsonExportOptions.EXCLUDE_MEASUREMENTS)
 def edgeJson=stream.toString(StandardCharsets.UTF_8)
 def decodedEdge=PathIO.readObjectsFromGeoJSON(new ByteArrayInputStream(edgeJson.getBytes(StandardCharsets.UTF_8)))[0]
 [name:edge.getName(),geojson:edgeJson,created_geometry:geometry(edge.getROI()),decoded_geometry:geometry(decodedEdge.getROI()),created_region:sampleObjects([edge])[0],decoded_region:sampleObjects([decodedEdge])[0]]
}
Files.writeString(root.resolve('edge-cases.json'),GsonTools.getInstance(true).toJson([schema:'qpx.qupath-edge-cases/1',cases:edgeCases])+'\n',StandardCharsets.UTF_8)

println('QPX_REFERENCE_OK '+objects.size())
