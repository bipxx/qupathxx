// Fresh-process witness: project classes + native and GeoJSON class semantics.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.ProjectIO
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools

def output=Path.of(args[0]);def gson=GsonTools.getInstance(true).newBuilder().serializeNulls().create()
def snapshot={pc -> pc==null?null:[path:pc.toList(),name:pc.getName(),parent:pc.getParentClass()?.toList(),argb:pc.getColor(),display:pc.toString()]}
def rows=[]
for(def name:['rgb','empty','rgba','null-color','color-conflict']) {
    def directory=output.resolve(name);def project=ProjectIO.loadProject(directory.resolve('project.qpproj').toFile(),BufferedImage.class)
    def row=[name:name,palette:project.getPathClasses().collect(snapshot),native:[],geojson:[]]
    for(def entry:project.getImageList()) {
        def data=PathIO.readImageData(entry.getEntryPath().resolve('data.qpdata'),entry.getServerBuilder())
        row.native.addAll(data.getHierarchy().getAnnotationObjects().collect{[id:it.getID().toString(),name:it.getName(),classification:snapshot(it.getPathClass()),area:it.getROI().getArea()]})
    }
    def geojson=directory.resolve('objects.geojson')
    if(Files.exists(geojson)) {
        try {row.geojson=PathIO.readObjects(geojson).collect{[id:it.getID().toString(),name:it.getName(),classification:snapshot(it.getPathClass()),area:it.getROI().getArea()]}}
        catch(Exception e){row.geojson_error=[type:e.getClass().getName(),message:e.getMessage()]}
    }
    rows.add(row);Files.writeString(output.resolve('reference.json'),gson.toJson(rows)+'\n')
}
