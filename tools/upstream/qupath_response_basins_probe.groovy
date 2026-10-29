// Only public QuPath/ImageJ APIs; no upstream segmentation implementation copied.
import ij.process.FloatProcessor
import qupath.imagej.processing.MorphologicalReconstruction
import qupath.imagej.processing.RoiLabeling
import qupath.imagej.processing.Watershed
import qupath.lib.io.GsonTools
import qupath.lib.common.GeneralTools
import java.nio.file.Path
import java.nio.file.Files
import java.nio.ByteBuffer
import java.nio.ByteOrder

def root=Path.of(args[0])
def cases=GsonTools.getInstance().fromJson(Files.readString(root.resolve('cases.json')),List.class)
def checks=[]
cases.each { item ->
    def bytes=ByteBuffer.wrap(Files.readAllBytes(root.resolve(item.name+'-input.f32le'))).order(ByteOrder.LITTLE_ENDIAN)
    float[] values=new float[(item.width as int)*(item.height as int)]
    for(int i=0;i<values.length;i++) values[i]=bytes.getFloat()
    def image=new FloatProcessor(item.width as int,item.height as int,values)
    def maxima=MorphologicalReconstruction.findRegionalMaxima(image,0.001f,false)
    def labels=RoiLabeling.labelImage(maxima,0,false)
    def seeds=ByteBuffer.allocate(values.length*4).order(ByteOrder.LITTLE_ENDIAN)
    for(int i=0;i<values.length;i++) seeds.putInt(labels.getf(i) as int)
    Files.write(root.resolve(item.name+'-seeds.u32le'),seeds.array())
    Watershed.doWatershed(image,labels,0,false)
    def output=ByteBuffer.allocate(values.length*4).order(ByteOrder.LITTLE_ENDIAN)
    for(int i=0;i<values.length;i++) output.putInt(labels.getf(i) as int)
    Files.write(root.resolve(item.name+'-labels.u32le'),output.array())
    checks.add([name:item.name,outside_is_nan:Float.isNaN(image.getPixelValue(-1,0)),
        outside_compares_nonzero:image.getPixelValue(-1,0)!=0f])
}
Files.writeString(root.resolve('runtime.json'),GsonTools.getInstance(true).toJson(
    [qupath_version:GeneralTools.getVersion(),java_version:System.getProperty('java.version'),cases:checks])+'\n')
