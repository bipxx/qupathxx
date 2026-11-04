import com.google.gson.GsonBuilder
import qupath.lib.objects.PathObjects
import qupath.lib.objects.classes.PathClass
import qupath.lib.objects.hierarchy.PathObjectHierarchy
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs
import qupath.lib.scripting.QP

def gson = new GsonBuilder().setPrettyPrinting().serializeNulls().create()
def folder = new File(args[0])
def inputs = gson.fromJson(new File(folder, 'input.json').text, List)
def results = inputs.collect { input ->
    def hierarchy = new PathObjectHierarchy()
    def objects = [:]
    input.nodes.each { n ->
        def roi
        if (n.rect != null) roi = ROIs.createRectangleROI(*n.rect.collect { ((Number)it).doubleValue() }, ImagePlane.getDefaultPlane())
        else roi = ROIs.createPointsROI(n.points.collect { ((Number)it[0]).doubleValue() } as double[], n.points.collect { ((Number)it[1]).doubleValue() } as double[], ImagePlane.getDefaultPlane())
        def classification = n.classification ? PathClass.fromString(n.classification) : null
        def obj = PathObjects.createAnnotationObject(roi, classification)
        obj.setName(n.token)
        obj.getMeasurementList().put('input measurement', 123.0)
        objects[n.token] = obj
    }
    input.nodes.each { n -> if (n.parent != null) objects[n.parent].addChildObject(objects[n.token]) }
    hierarchy.addObjects(input.nodes.findAll { it.parent == null }.collect { objects[it.token] })
    def selected = input.selected.collect { objects[it] }
    hierarchy.getSelectionModel().setSelectedObjects(selected, selected[0])
    def result = [name:input.name]
    try {
        result.changed = QP.mergeAnnotations(hierarchy, selected)
        def merged = hierarchy.getSelectionModel().getSelectedObject()
        def token = { obj -> obj == null || obj.isRootObject() ? null : objects.find { k,v -> v.is(obj) }?.key ?: 'merged' }
        result.new_identity = !objects.values().any { it.is(merged) }
        result.geometry = merged.getROI().getGeometry().toText()
        result.area = merged.getROI().getArea()
        result.classification = merged.getPathClass()?.toString()
        result.object_name = merged.getName()
        result.measurements = merged.getMeasurementList().size()
        result.selected = hierarchy.getSelectionModel().getSelectedObjects().collect { token(it) }.sort()
        result.objects = hierarchy.getFlattenedObjectList(null).findAll { !it.isRootObject() }.collect { obj ->
            [token:token(obj), parent:token(obj.getParent()), classification:obj.getPathClass()?.toString()]
        }.sort { it.token }
    } catch (Exception e) {
        result.error = e.getClass().getSimpleName()
        result.message = e.getMessage()
    }
    result
}
new File(folder, 'reference.json').text = gson.toJson(results) + '\n'
