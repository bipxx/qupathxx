// Pinned QuPath0.7 image analysis settings; one output directory argument.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.projects.Projects
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.io.GsonTools
import qupath.lib.color.ColorDeconvolutionStains
import qupath.lib.color.StainVector
import qupath.lib.objects.PathObjects
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs

def output=Path.of(args[0]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class);def rows=[]
def kinds=ImageData.ImageType.values().toList()
(kinds.collect{[it.name(),it]}+[['custom-three',ImageData.ImageType.BRIGHTFIELD_OTHER],['wide-white',ImageData.ImageType.BRIGHTFIELD_H_E]]).each {entrySpec ->
    def server=ImageServerProvider.buildServer(output.resolve('source.ome.tif').toString(),BufferedImage.class)
    def entry=project.addImage(server.getBuilder());entry.setImageName(entrySpec[0])
    def data=new ImageData(server);data.setImageType(entrySpec[1])
    if(entrySpec[0]=='custom-three')data.setColorDeconvolutionStains(new ColorDeconvolutionStains('Custom α',StainVector.createStainVector('red',1,0,0),StainVector.createStainVector('green',0,1,0),StainVector.createStainVector('blue',0,0,1),240,245,250))
    if(entrySpec[0]=='wide-white')data.setColorDeconvolutionStains(new ColorDeconvolutionStains('Quantitative reference',StainVector.createStainVector('A',.65,.7,.29),StainVector.createStainVector('B',.27,.57,.78),65535,65534,65533))
    def object=PathObjects.createAnnotationObject(ROIs.createRectangleROI(1,2,8,9,ImagePlane.getDefaultPlane()));object.setName('ROI');data.getHierarchy().getRootObject().addChildObject(object)
    entry.saveImageData(data)
    // Reference the values actually read from the saved record, not pre-save state.
    def restored=entry.readImageData();def stains=restored.getColorDeconvolutionStains()
    def row=[name:entrySpec[0],image_type:restored.getImageType().name(),stains:null]
    if(stains!=null)row.stains=[name:stains.getName(),names:(1..3).collect{stains.getStain(it).getName()},basis:(1..3).collect{stains.getStain(it).getArray().toList()},white:[stains.getMaxRed(),stains.getMaxGreen(),stains.getMaxBlue()],residual:stains.getStain(3).isResidual()]
    rows.add(row);restored.getServer().close();server.close()
}
project.syncChanges();Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(rows)+'\n')
