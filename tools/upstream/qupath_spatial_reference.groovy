// Synthetic spatial oracle using the pinned QuPath runtime, without qupathxx.
// args[0]: contract JSON; args[1]: a new output directory.
import qupath.lib.analysis.DistanceTools
import qupath.lib.common.GeneralTools
import qupath.lib.io.GsonTools
import qupath.lib.objects.PathObjects
import qupath.lib.objects.PathObjectTools
import qupath.lib.measurements.MeasurementListFactory
import qupath.lib.measurements.MeasurementList.MeasurementListType
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path

def contract = GsonTools.getInstance().fromJson(Files.readString(Path.of(args[0])), Map)
def root = Path.of(args[1])
if (Files.exists(root)) throw new IllegalArgumentException('Output must be new')
Files.createDirectories(root)
def makeRoi = { spec, plane ->
    switch (spec.kind) {
        case 'rectangle':
            return ROIs.createRectangleROI(*spec.bounds.collect { it.doubleValue() }, plane)
        case 'ellipse':
            return ROIs.createEllipseROI(*spec.bounds.collect { it.doubleValue() }, plane)
        case 'points':
            return ROIs.createPointsROI(spec.points.collect { it[0].doubleValue() } as double[],
                spec.points.collect { it[1].doubleValue() } as double[], plane)
        case 'polygon':
            return ROIs.createPolygonROI(spec.points.collect { it[0].doubleValue() } as double[],
                spec.points.collect { it[1].doubleValue() } as double[], plane)
        case 'polyline':
            return ROIs.createPolylineROI(spec.points.collect { it[0].doubleValue() } as double[],
                spec.points.collect { it[1].doubleValue() } as double[], plane)
        default: throw new IllegalArgumentException('Unknown geometry ' + spec.kind)
    }
}
def observed = contract.cases.collect { c ->
    def specs = new ArrayList(c.objects)
    def targetIds = new ArrayList(c.targets)
    if (c.generated_targets) {
        def g = c.generated_targets
        (0..<g.count.intValue()).each { i ->
            def id = g.prefix + i
            specs.add([id: id, body: [kind: 'points', points: [[g.start[0] + i*g.step[0], g.start[1] + i*g.step[1]]]]])
            targetIds.add(id)
        }
    }
    def objects = specs.collectEntries { spec ->
        def plane = ImagePlane.getPlane((spec.z ?: 0).intValue(), (spec.t ?: 0).intValue())
        def body = makeRoi(spec.body, plane)
        def measurements = spec.default_float ? null :
            MeasurementListFactory.createMeasurementList(0,
                MeasurementListType.valueOf(spec.storage ?: 'GENERAL'))
        def obj = (spec.nucleus || spec.object_type == 'cell') ?
            PathObjects.createCellObject(body, spec.nucleus ? makeRoi(spec.nucleus, plane) : null, null, measurements) :
            PathObjects.createDetectionObject(body, null, measurements)
        obj.setName(spec.id)
        [(spec.id): obj]
    }
    def source = objects[c.source]
    def targets = targetIds.findAll { !c.exclude_self || it != c.source }.collect { objects[it] }
    DistanceTools.centroidToCentroidDistance2D([source], targets,
        c.scale[0].doubleValue(), c.scale[1].doubleValue(), 'fixture_distance')
    def value = source.getMeasurementList().get('fixture_distance')
    def centroid = { obj ->
        def roi = PathObjectTools.getROI(obj, true)
        [roi.getCentroidX(), roi.getCentroidY()]
    }
    [name: c.name, measurement_storage: source.getMeasurementList().getClass().getName(), source_centroid: centroid(source),
        target_centroids: targets.collect { centroid(it) },
        measured: Double.isFinite(value), distance: Double.isFinite(value) ? value : null]
}
def reference = [schema: 'qpx.qupath-spatial-reference/1',
    qupath_version: GeneralTools.getVersion(), upstream_revision: contract.upstream_revision,
    java_version: System.getProperty('java.version'), synthetic: true, cases: observed]
def json = new com.google.gson.GsonBuilder().serializeNulls().setPrettyPrinting().create()
Files.writeString(root.resolve('reference.json'), json.toJson(reference) + '\n', StandardCharsets.UTF_8)
println('QPX_SPATIAL_REFERENCE_OK ' + observed.size())
