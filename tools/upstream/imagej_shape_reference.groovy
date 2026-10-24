// Oracle capture through the pinned public ImageJ 1.54p APIs.
import ij.IJ
import ij.process.ByteProcessor
import ij.process.ImageProcessor
import ij.plugin.filter.EDM
import ij.plugin.filter.MaximumFinder
import qupath.lib.io.GsonTools
import java.nio.file.Files
import java.nio.file.Path
import java.util.zip.GZIPInputStream
import java.util.zip.GZIPOutputStream

def root=Path.of(args[0])
def contract=GsonTools.getInstance().fromJson(Files.readString(root.resolve('contract.json')),Map.class)
if(IJ.getVersion()!=contract.imagej_version) throw new IllegalStateException('ImageJ runtime version differs')
def records=[]
contract.cases.each {entry ->
    int width=entry.width,height=entry.height
    byte[] input=new GZIPInputStream(Files.newInputStream(root.resolve(entry.name+'/input.u8.gz'))).withCloseable {it.readAllBytes()}
    if(width<=0 || height<=0 || (long)width*height>1024*1024 || input.length!=width*height)
        throw new IllegalArgumentException('Invalid shape canvas')
    for(int i=0;i<input.length;i++) input[i]=input[i]==0?0:(byte)255
    def mask=new ByteProcessor(width,height,input,null)
    def edm=new EDM().makeFloatEDM(mask,0,false)
    def maxima=new MaximumFinder().findMaxima(edm,.5,ImageProcessor.NO_THRESHOLD,MaximumFinder.SINGLE_POINTS,false,true)
    new EDM().toWatershed(mask)
    int foreground=0,points=0
    ['mask':mask,'maxima':maxima].each {name,image ->
        new GZIPOutputStream(Files.newOutputStream(root.resolve(entry.name+'/'+name+'.u8.gz'))).withCloseable {out ->
            for(int i=0;i<input.length;i++) out.write(image.get(i)==0?0:1)
        }
    }
    for(int i=0;i<input.length;i++) {if(mask.get(i)!=0) foreground++;if(maxima.get(i)!=0) points++}
    records.add([name:entry.name,foreground:foreground,maxima:points])
}
Files.writeString(root.resolve('reference.json'),GsonTools.getInstance(true).toJson([imagej_version:IJ.getVersion(),cases:records])+'\n')
println('QPX_SHAPE_REFERENCE_OK '+records.size())
