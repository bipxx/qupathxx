// Capture the exact post-threshold primitive used by pinned QuPath0.7.0.
import ij.IJ
import ij.Prefs
import ij.process.ByteProcessor
import ij.plugin.filter.RankFilters
import qupath.lib.io.GsonTools
import java.nio.file.Files
import java.nio.file.Path

def folder=Path.of(args[0])
def gson=GsonTools.getInstance()
def contract=gson.fromJson(Files.readString(folder.resolve('median-contract.json')),Map.class)
if(IJ.getVersion()!=contract.imagej_version) throw new IllegalStateException('ImageJ version differs')
Prefs.setThreads(1)
def records=contract.cases.collect { entry ->
    int width=entry.width,height=entry.height
    String bits=entry.bits
    byte[] pixels=new byte[width*height]
    for(int i=0;i<pixels.length;i++) pixels[i]=(byte)(bits.charAt(i)=='1' as char?255:0)
    def image=new ByteProcessor(width,height,pixels,null)
    new RankFilters().rank(image,1d,RankFilters.MEDIAN)
    def output=new StringBuilder(pixels.length)
    for(byte value:(byte[])image.getPixels()) output.append((value&255)!=0?'1':'0')
    [name:entry.name,width:width,height:height,bits:bits,median:output.toString()]
}
Files.writeString(folder.resolve('median-reference.json'),gson.toJson([imagej_version:IJ.getVersion(),radius:1,cases:records])+'\n')
println 'Median reference cases: '+records.size()
