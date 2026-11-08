// QuPath0.7 project-entry metadata only; no saved ImageData/ROI recapture.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.projects.ProjectIO
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.io.GsonTools

def output=Path.of(args[0]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class)
def rich=['patient':'Synthetic α 🚀','batch':'A','empty':'','':'empty key','multiline':'line1\nline2\t"quoted"','integer':'9223372036854775807','null-word':'null','é':'composed','e\u0301':'decomposed']
for(def label:['metadata α','empty','with-losses']) {
    def server=ImageServerProvider.buildServer(output.resolve('source.ome.tif').toString(),BufferedImage.class)
    try {
        def entry=project.addImage(server.getBuilder());entry.setImageName(label)
        if(label!='empty')entry.getMetadata().putAll(rich)
        if(label=='with-losses') {entry.setDescription('Free text stays a distinct field');entry.getTags().add('research-tag')}
    } finally {server.close()}
}
project.syncChanges()
def restored=ProjectIO.loadProject(project.getPath().toFile(),BufferedImage.class)
def rows=restored.getImageList().collect{[id:it.getID(),name:it.getImageName(),metadata:new TreeMap(it.getMetadata()),description:it.getDescription(),tags:it.getTags().toList().sort()]}
Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(rows)+'\n')
