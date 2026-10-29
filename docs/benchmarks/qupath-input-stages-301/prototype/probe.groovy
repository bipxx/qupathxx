import ij.process.ColorProcessor
import qupath.imagej.tools.IJTools
import qupath.lib.color.ColorDeconvolutionHelper
import qupath.lib.color.ColorDeconvolutionStains
import qupath.lib.color.StainVector
import qupath.lib.io.GsonTools
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files
import java.nio.file.Path
import java.util.zip.GZIPOutputStream
def root=Path.of(args[0]);def gson=GsonTools.getInstance(true)
def cases=gson.fromJson(Files.readString(root.resolve('cases.json')),List)
def writeFloats={name,values ->
    def bytes=ByteBuffer.allocate(values.length*4).order(ByteOrder.LITTLE_ENDIAN)
    for(float value:values)bytes.putFloat(value)
    def stream=new GZIPOutputStream(Files.newOutputStream(root.resolve(name)))
    try {stream.write(bytes.array())}finally{stream.close()}
}
for(item in cases) {
    int w=item.width,h=item.height
    def raw=Files.readAllBytes(root.resolve(item.image+'.rgb'));int[] pixels=new int[w*h]
    for(int i=0;i<pixels.length;i++)pixels[i]=((raw[3*i]&255)<<16)|((raw[3*i+1]&255)<<8)|(raw[3*i+2]&255)
    def cp=new ColorProcessor(w,h,pixels)
    def vector=[];item.vectors.eachWithIndex {v,index -> vector.add(StainVector.createStainVector('S'+index,*(v.collect {it as double})))}
    def stains=vector.size()==2 ? new ColorDeconvolutionStains(item.name,vector[0],vector[1],*(item.white.collect {it as double})) :
        new ColorDeconvolutionStains(item.name,vector[0],vector[1],vector[2],*(item.white.collect {it as double}))
    for(int c=0;c<3;c++) {
        def fp=cp.toFloat(c,null)
        ColorDeconvolutionHelper.convertPixelsToOpticalDensities((float[])fp.getPixels(),item.white[c] as double,true)
        writeFloats(item.name+'-od'+c+'.f32le.gz',fp.getPixels())
    }
    writeFloats(item.name+'-sum.f32le.gz',IJTools.convertToOpticalDensitySum(cp,*(item.white.collect {it as double})).getPixels())
    def separated=IJTools.colorDeconvolve(cp,stains)
    for(int c=0;c<3;c++)writeFloats(item.name+'-stain'+c+'.f32le.gz',separated[c].getPixels())
    Files.writeString(root.resolve(item.name+'-model.json'),gson.toJson([normalized:(1..3).collect {stains.getStain(it).getArray()},inverse:stains.getMatrixInverse()])+'\n')
}
