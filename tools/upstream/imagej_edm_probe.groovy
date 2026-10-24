// Stage harness through the pinned runtime's public ImageJ/QuPath APIs.
import ij.process.ByteProcessor
import ij.process.FloatProcessor
import ij.plugin.filter.EDM
import ij.gui.Wand
import qupath.imagej.processing.RoiLabeling
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files
import java.nio.file.Path

def input=ByteBuffer.wrap(Files.readAllBytes(Path.of(args[0]))).order(ByteOrder.LITTLE_ENDIAN)
byte[] magic=new byte[4];input.get(magic)
if(new String(magic,'US-ASCII')!='QLR1') throw new IllegalArgumentException('Unknown raster protocol')
int width=input.getInt(),height=input.getInt();input.getInt()
if(width<=0 || height<=0 || (long)width*height>1024*1024 || input.remaining()!=(long)width*height*4)
    throw new IllegalArgumentException('Invalid raster dimensions')
def mask=new ByteProcessor(width,height)
for(int i=0;i<width*height;i++) mask.set(i,input.getInt()==0?0:255)
new EDM().toWatershed(mask)
mask.setThreshold(127,Double.POSITIVE_INFINITY,0)
def regions=RoiLabeling.getFilledPolygonROIs(mask,Wand.FOUR_CONNECTED)
def labels=new FloatProcessor(width,height)
RoiLabeling.labelROIs(labels,regions)
def output=ByteBuffer.allocate(16+width*height*4).order(ByteOrder.LITTLE_ENDIAN)
output.put('QLR1'.getBytes('US-ASCII'));output.putInt(width);output.putInt(height);output.putInt(regions.size())
for(int i=0;i<width*height;i++) output.putInt((int)labels.getf(i))
Files.write(Path.of(args[1]),output.array())
println('QPX_IMAGEJ_EDM_OK '+regions.size())
