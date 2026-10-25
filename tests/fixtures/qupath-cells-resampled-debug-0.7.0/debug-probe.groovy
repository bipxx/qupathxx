// Retain the plugin's own public debug stack through ImageJ's public listener.
import ij.ImagePlus
import ij.ImageListener
import ij.io.FileSaver
import qupath.imagej.detect.cells.WatershedCellDetection
import qupath.lib.io.GsonTools
import java.nio.file.Files
import java.nio.file.Path

def root=Path.of(args[0])
def listener=[imageOpened:{ ImagePlus image ->
    if(!image.getTitle().startsWith('Debug stack')) return
    def stack=image.getStack()
    def metadata=[width:image.getWidth(),height:image.getHeight(),slices:(1..stack.getSize()).collect {stack.getSliceLabel(it)}]
    if(!new FileSaver(image).saveAsTiffStack(root.resolve('debug-stack.tif').toString()))
        throw new IllegalStateException('Unable to save debug stack')
    Files.writeString(root.resolve('debug-stack.json'),GsonTools.getInstance(true).toJson(metadata)+'\n')
    image.changes=false;image.close()
}, imageClosed:{ ImagePlus image -> }, imageUpdated:{ ImagePlus image -> }] as ImageListener
ImagePlus.addImageListener(listener)
WatershedCellDetection.setDebugMode(true)
try {evaluate(new File(args[2]))}
finally {WatershedCellDetection.setDebugMode(false);ImagePlus.removeImageListener(listener)}
