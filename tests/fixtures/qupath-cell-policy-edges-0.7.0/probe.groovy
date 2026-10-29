// Observe public ImageJ debug stacks; evaluate the retained public-API driver.
import ij.ImagePlus
import ij.ImageListener
import ij.io.FileSaver
import qupath.imagej.detect.cells.WatershedCellDetection
import qupath.lib.io.GsonTools
import java.nio.file.Files
import java.nio.file.Path

def root=Path.of(args[0])
binding.setVariable('captureFolder',root)
def listener=[imageOpened:{ ImagePlus image ->
    if(!image.getTitle().startsWith('Debug stack')) return
    def folder=binding.getVariable('captureFolder')
    def stack=image.getStack()
    def metadata=[width:image.getWidth(),height:image.getHeight(),
        slices:(1..stack.getSize()).collect {stack.getSliceLabel(it)}]
    if(!new FileSaver(image).saveAsTiffStack(folder.resolve('processing-stack.tif').toString()))
        throw new IllegalStateException('Cannot retain processing stages')
    Files.writeString(folder.resolve('processing-stack.json'),GsonTools.getInstance(true).toJson(metadata)+'\n')
    image.changes=false;image.close()
},imageClosed:{ ImagePlus image -> },imageUpdated:{ ImagePlus image -> }] as ImageListener
ImagePlus.addImageListener(listener)
try {evaluate(root.resolve('reference.groovy').toFile())}
finally {WatershedCellDetection.setDebugMode(false);ImagePlus.removeImageListener(listener)}
