// Public ImageJ operations only; no plugin internals or replacement kernel.
import ij.IJ
import ij.process.FloatProcessor
import ij.plugin.filter.GaussianBlur
import qupath.lib.io.GsonTools
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files
import java.nio.file.Path

def root=Path.of(args[0])
def gson=GsonTools.getInstance(true)
def cases=gson.fromJson(Files.readString(root.resolve('cases.json')),List)
def writeFloats={path, values ->
    def bytes=ByteBuffer.allocate(values.length*4).order(ByteOrder.LITTLE_ENDIAN)
    for(float value:values) bytes.putFloat(value)
    Files.write(path,bytes.array())
}
for(item in cases) {
    int w=item.width,h=item.height
    def buffer=ByteBuffer.wrap(Files.readAllBytes(root.resolve(item.name+'-input.f32le'))).order(ByteOrder.LITTLE_ENDIAN)
    float[] values=new float[w*h]
    for(int i=0;i<values.length;i++) values[i]=buffer.getFloat()
    def fp=new FloatProcessor(w,h,values)
    fp.blurGaussian(item.sigma as double)
    writeFloats(root.resolve(item.name+'-blur.f32le'),fp.getPixels())
    fp.convolve([0f,-1f,0f,-1f,4f,-1f,0f,-1f,0f] as float[],3,3)
    writeFloats(root.resolve(item.name+'-log.f32le'),fp.getPixels())
    // This requested-sigma kernel is diagnostic. Large-sigma blur may rescale.
    def kernel=new GaussianBlur().makeGaussianKernel(item.sigma as double,0.0002d,Math.max(w,h))
    Files.writeString(root.resolve(item.name+'-kernel.json'),gson.toJson(kernel)+'\n')
}
Files.writeString(root.resolve('runtime.json'),gson.toJson([
    imagej_version:IJ.getVersion(),java_version:System.getProperty('java.version')])+'\n')
