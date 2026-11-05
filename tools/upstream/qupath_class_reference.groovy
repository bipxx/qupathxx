// Class-only supplement: retained builder/ROI, lazy saved-data IO; no pixels.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.projects.ProjectIO
import qupath.lib.objects.PathObjects
import qupath.lib.objects.classes.PathClass
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools

def parent=ProjectIO.loadProject(Path.of(args[0]).toFile(),BufferedImage.class)
def original=parent.getImageList().get(0);def builder=original.getServerBuilder()
def output=Path.of(args[1]);def gson=GsonTools.getInstance(true).newBuilder().serializeNulls().create()
def snapshot={ pc -> [path:pc.toList(),name:pc.getName(),parent:pc.getParentClass()?.toList(),argb:pc.getColor(),display:pc.toString()] }
def root=PathClass.getInstance('Tumor α',0x113355)
def positive=PathClass.getInstance(root,'Positive',0xaa2211)
def negative=PathClass.getInstance(root,'Negative',0x4477aa)
def deep=PathClass.getInstance(positive,'High',0x778899)
def unicode=PathClass.getInstance('Cell µ 🚀',0xabcdef)
def literal=PathClass.getInstance('Marker / label #1',0x123456)
def alpha=PathClass.getInstance(root,'Alpha',(int)0x80123456L)
def cleared=PathClass.getInstance('Cleared color',0x89abcd);cleared.setColor((Integer)null)
def conflict=PathClass.getInstance('Conflict class',0x101112)
def rgb=[deep,root,negative,positive,unicode,literal,deep,PathClass.NULL_CLASS]
def specs=[
    [name:'rgb',palette:rgb,objects:[root,positive,negative,deep,unicode,literal]],
    [name:'empty',palette:rgb,objects:[]],
    [name:'rgba',palette:[root,alpha],objects:[alpha]],
    [name:'null-color',palette:[cleared],objects:[cleared]],
    [name:'color-conflict',palette:[conflict],objects:[conflict]]
]
def rejected=[]
for(def name:['Literal: colon','line\nbreak']) {
    try {PathClass.getInstance(name);rejected.add([name:name,rejected:false])}
    catch(IllegalArgumentException e){rejected.add([name:name,rejected:true,error:e.getMessage()])}
}
def rows=[]
for(def spec:specs) {
    def directory=output.resolve(spec.name);Files.createDirectory(directory)
    def project=Projects.createProject(directory.toFile(),BufferedImage.class)
    def row=[name:spec.name,palette_input:spec.palette.collect(snapshot),objects:[]]
    if(!spec.objects.isEmpty()) {
        def entry=project.addImage(builder);entry.setImageName('Classes '+spec.name)
        def data=PathIO.readImageData(original.getEntryPath().resolve('data.qpdata'),builder)
        def roi=data.getHierarchy().getAnnotationObjects().iterator().next().getROI()
        data.getHierarchy().clearAll();def objects=[]
        spec.objects.eachWithIndex{pc,index ->
            def object=PathObjects.createAnnotationObject(roi,pc);object.setName(spec.name+' '+index);objects.add(object)
            row.objects.add([id:object.getID().toString(),name:object.getName(),classification:snapshot(pc),area:roi.getArea()])
        }
        data.getHierarchy().getRootObject().addChildObjects(objects)
        Files.createDirectories(entry.getEntryPath())
        PathIO.writeImageData(entry.getEntryPath().resolve('data.qpdata'),data)
        PathIO.exportObjectsAsGeoJSON(directory.resolve('objects.geojson'),objects,PathIO.GeoJsonExportOptions.FEATURE_COLLECTION,PathIO.GeoJsonExportOptions.PRETTY_JSON)
        if(spec.name=='color-conflict')conflict.setColor(0xa0b0c0)
    }
    project.setPathClasses(spec.palette);project.syncChanges()
    row.palette_saved=project.getPathClasses().collect(snapshot);rows.add(row)
    Files.writeString(output.resolve('producer-reference.json'),gson.toJson([delimiter:PathClass.DELIMITER,cases:rows,rejected_names:rejected])+'\n')
}
