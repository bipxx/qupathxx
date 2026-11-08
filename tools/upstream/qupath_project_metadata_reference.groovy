// Project-only supplement: retained builder; no image server/pixel/ROI reads.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.projects.ProjectIO
import qupath.lib.io.GsonTools

def parent=ProjectIO.loadProject(Path.of(args[0]).toFile(),BufferedImage.class)
def output=Path.of(args[1]);def builder=parent.getImageList().get(0).getServerBuilder()
def rich=['project-key':'project-scope','':'empty key','empty':'','multiline':'line1\nline2\t"quoted"','unicode':'Synthetic α 🚀','é':'composed','e\u0301':'decomposed','integer':'9223372036854775807','null-word':'null']
def specs=[[name:'rich',metadata:rich,images:true],[name:'empty',metadata:[:],images:true],[name:'no-images',metadata:rich,images:false]]
def rows=[]
for(def spec:specs) {
    def directory=output.resolve(spec.name);Files.createDirectory(directory)
    def project=Projects.createProject(directory.toFile(),BufferedImage.class)
    project.getMetadata().putAll(spec.metadata)
    if(spec.images) {def entry=project.addImage(builder);entry.setImageName('Entry');entry.getMetadata().put('project-key','entry-scope')}
    project.syncChanges()
    def restored=ProjectIO.loadProject(project.getPath().toFile(),BufferedImage.class)
    rows.add([name:spec.name,metadata:new LinkedHashMap(restored.getMetadata()),images:restored.getImageList().collect{[id:it.getID(),name:it.getImageName(),metadata:new LinkedHashMap(it.getMetadata())]}])
}
Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).toJson(rows)+'\n')
