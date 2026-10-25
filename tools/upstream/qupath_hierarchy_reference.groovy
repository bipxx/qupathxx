// Pinned QuPath v0.7.0 hierarchy/loss oracle; never invokes qupathxx.
// args[0]: new output directory; optional args[1]: native GeoJSON to inspect.
import qupath.lib.common.ColorTools
import qupath.lib.common.GeneralTools
import qupath.lib.io.GsonTools
import qupath.lib.io.PathIO
import qupath.lib.objects.PathObjects
import qupath.lib.objects.classes.PathClass
import qupath.lib.objects.hierarchy.PathObjectHierarchy
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path

def root = Path.of(args[0])
if (Files.exists(root)) throw new IllegalArgumentException('Output must be new')
Files.createDirectories(root)
def gson = GsonTools.getInstance(true)
def plane = ImagePlane.getDefaultPlane()
def cls = PathClass.getInstance('Tumor', ColorTools.packRGB(255,32,48))
def make = { x,y,w,h -> ROIs.createRectangleROI(x,y,w,h,plane) }
def parent = PathObjects.createAnnotationObject(make(8,8,180,150),cls)
def region = PathObjects.createAnnotationObject(make(20,20,80,80),cls)
def cell = PathObjects.createCellObject(make(30,30,20,16),make(35,34,5,4),cls,null)
def detection = PathObjects.createDetectionObject(make(120,30,8,12),cls)
def sibling = PathObjects.createAnnotationObject(make(210,20,25,30))
def points = PathObjects.createAnnotationObject(ROIs.createPointsROI([215d,220d] as double[],[25d,30d] as double[],plane))
def all = [parent,region,cell,detection,sibling,points]
all.eachWithIndex { o,i ->
    o.setID(UUID.fromString(String.format('10000000-0000-0000-0000-%012d',i+1)))
    o.setName(['Parent α','Region','Cell','Detection','Sibling','Points'][i])
}
parent.addChildObject(region); region.addChildObject(cell)
parent.addChildObject(detection); sibling.addChildObject(points)
def hierarchy = new PathObjectHierarchy()
hierarchy.getRootObject().addChildObjects([parent,sibling])

// Public hierarchy serialization emits childObjects. The root has no ROI;
// wrap its actual serialized children as image-coordinate GeoJSON features.
def raw = gson.toJson(hierarchy)
Files.writeString(root.resolve('hierarchy.json'),raw+'\n',StandardCharsets.UTF_8)
def serialized = com.google.gson.JsonParser.parseString(raw)
def collection = new com.google.gson.JsonObject()
collection.addProperty('type','FeatureCollection')
collection.add('features',serialized.getAsJsonObject('root').getAsJsonObject('properties').getAsJsonArray('childObjects'))
Files.writeString(root.resolve('nested.geojson'),gson.toJson(collection)+'\n',StandardCharsets.UTF_8)
PathIO.exportObjectsAsGeoJSON(root.resolve('flat.geojson'),all,PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,PathIO.GeoJsonExportOptions.EXCLUDE_MEASUREMENTS,PathIO.GeoJsonExportOptions.PRETTY_JSON)

def snapshot
snapshot = { items ->
    def result=[]
    def visit
    visit = { o,p ->
        def r=o.getROI(); def n=o.isCell()?o.getNucleusROI():null
        result.add([id:o.getID().toString(),parent:p,name:o.getName(),kind:o.isCell()?'cell':o.isDetection()?'detection':'annotation',classification:o.getPathClass()?.toString(),class_color:o.getPathClass()?.getColor(),bounds:[r.getBoundsX(),r.getBoundsY(),r.getBoundsWidth(),r.getBoundsHeight()],area:r.getArea(),nucleus_bounds:n ? [n.getBoundsX(),n.getBoundsY(),n.getBoundsWidth(),n.getBoundsHeight()] : null,locked:o.isLocked(),measurement_count:o.getMeasurementList().size(),metadata:o.getMetadata()])
        o.getChildObjects().each { visit(it,o.getID().toString()) }
    }
    items.each { visit(it,null) }
    result
}
def nested = PathIO.readObjects(root.resolve('nested.geojson'))
def flat = PathIO.readObjects(root.resolve('flat.geojson'))

// Capture genuinely unsupported properties separately from the clean hierarchy.
def lossy=PathObjects.createAnnotationObject(make(10,10,20,20),cls)
lossy.setName('Unsupported properties');lossy.setID(UUID.fromString('20000000-0000-0000-0000-000000000001'))
lossy.setLocked(true);lossy.setColor(ColorTools.packRGB(12,34,56))
lossy.getMeasurementList().put('Fixture measurement',12.5)
PathIO.exportObjectsAsGeoJSON(root.resolve('losses.geojson'),[lossy],PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,PathIO.GeoJsonExportOptions.PRETTY_JSON)
// QuPath 0.7.0 writes metadata twice in this path. Preserve raw bytes so a
// duplicate-rejecting native reader cannot silently accept rewritten evidence.
lossy.getMetadata().put('fixture','retain raw duplicate members')
PathIO.exportObjectsAsGeoJSON(root.resolve('metadata-raw.geojson'),[lossy],PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,PathIO.GeoJsonExportOptions.PRETTY_JSON)
def reference=[schema:'qpx.qupath-hierarchy-reference/1',qupath_version:GeneralTools.getVersion(),upstream_revision:'04ccfa4fb7d43e9b566393e08e83690b72248d44',java_version:System.getProperty('java.version'),image_size:[256,192],synthetic:true,created:snapshot([parent,sibling]),nested_decoded:snapshot(nested),flat_decoded:snapshot(flat),losses:snapshot([lossy])]
if(args.length>1) reference.native_decoded=snapshot(PathIO.readObjects(Path.of(args[1])))
Files.writeString(root.resolve('reference.json'),gson.toJson(reference)+'\n',StandardCharsets.UTF_8)
println('QPX_HIERARCHY_REFERENCE_OK 6')
