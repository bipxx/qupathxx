import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.io.GsonTools
import qupath.lib.objects.PathObjects
import qupath.lib.objects.classes.PathClass
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs

def output=Path.of(args[0])
def projectDir=output.resolve('project');Files.createDirectory(projectDir)
def project=Projects.createProject(projectDir.toFile(),BufferedImage.class)
def cls=PathClass.fromString('Tumor');cls.setColor(0xff2030)
project.setPathClasses([cls])
def summaries=[]
['slide α.ome.tif','gray.ome.tif'].eachWithIndex { name,i ->
    def server=ImageServerProvider.buildServer(output.resolve('sources').resolve(name).toString(),BufferedImage.class)
    def entry=project.addImage(server.getBuilder());entry.setImageName(['Slide α','Gray specimen'][i])
    def data=new ImageData(server)
    def plane=ImagePlane.getDefaultPlane()
    def parent=PathObjects.createAnnotationObject(ROIs.createRectangleROI(2,2,50,40,plane),cls)
    parent.setName('Parent α')
    def child=PathObjects.createAnnotationObject(ROIs.createRectangleROI(4,4,10,10,plane),cls);child.setName('Child')
    parent.addChildObject(child);data.getHierarchy().getRootObject().addChildObject(parent)
    entry.saveImageData(data)
    summaries.add([entry_id:entry.getID(),name:entry.getImageName(),source:server.getURIs().collect {it.toString()},objects:data.getHierarchy().nObjects(),pixel_width_um:server.getPixelCalibration().getPixelWidthMicrons(),pixel_height_um:server.getPixelCalibration().getPixelHeightMicrons()])
    server.close()
}
project.syncChanges()
Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).toJson([project:project.getPath().toString(),images:summaries])+'\n')
