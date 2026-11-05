// Entry-only supplement: reuse a captured builder; no server/pixel/ROI recapture.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.projects.ProjectIO
import qupath.lib.io.GsonTools

def source=ProjectIO.loadProject(Path.of(args[0]).toFile(),BufferedImage.class)
def output=Path.of(args[1]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class)
def builder=source.getImageList().get(0).getServerBuilder()
def cases=[
    [name:'null-description',description:null,tags:[]],
    [name:'empty-description',description:'',tags:['z-last','a-first','z-last','','µ','α']],
    [name:'unicode-description',description:'Description α 🚀\nSecond line\t"quoted"',tags:['Tumor','tumor','e\u0301','é','Tumor']]
]
for(def spec:cases) {
    def entry=project.addImage(builder);entry.setImageName(spec.name);entry.setDescription(spec.description);entry.getTags().addAll(spec.tags)
}
project.syncChanges()
def restored=ProjectIO.loadProject(project.getPath().toFile(),BufferedImage.class)
def rows=restored.getImageList().collect{[id:it.getID(),name:it.getImageName(),description:it.getDescription(),tags:it.getTags().toList()]}
Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(rows)+'\n')
