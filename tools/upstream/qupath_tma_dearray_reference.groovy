// Characterize the pinned public detector on independently authored binary masks.
// No upstream detector implementation is copied or changed.
import qupath.imagej.detect.dearray.TMADearrayer
import qupath.lib.common.GeneralTools
import qupath.lib.io.GsonTools
import ij.process.ByteProcessor
import java.awt.Polygon
import java.nio.file.Files
import java.nio.file.Path
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.util.Arrays

def root=Path.of(args[0]);def output=Path.of(args[1])
if(Files.exists(output))throw new IllegalStateException('reference output already exists')
if(GeneralTools.getVersion()!='0.7.0')throw new IllegalStateException('wrong upstream version')
def gson=GsonTools.getInstance(true)
def contract=gson.fromJson(Files.readString(root.resolve('contract.json')),Map.class)
def field={object,name -> def f=object.getClass().getDeclaredField(name);f.setAccessible(true);f.get(object)}
def density=TMADearrayer.class.getDeclaredMethod('computeDensities',ByteProcessor.class,Polygon.class,Double.TYPE)
density.setAccessible(true)
def observations=[]
for(item in contract.cases) {
    def raw=Files.readAllBytes(root.resolve(item.mask))
    def sha=MessageDigest.getInstance('SHA-256').digest(raw).encodeHex().toString()
    if(sha!=item.mask_sha256)throw new IllegalStateException('fixture checksum differs')
    int width=item.width.intValue(),height=item.height.intValue()
    def header="P5\n${width} ${height}\n255\n".getBytes(StandardCharsets.US_ASCII)
    if(raw.length!=header.length+width*height || !Arrays.equals(Arrays.copyOfRange(raw,0,header.length),header))throw new IllegalStateException('invalid binary fixture')
    def bp=new ByteProcessor(width,height,Arrays.copyOfRange(raw,header.length,raw.length),null)
    def result=[name:item.name,mask_sha256:sha]
    try {
        def shape=TMADearrayer.detectTMACoresFromBinary(bp,item.upstream_core_diameter.doubleValue(),item.columns.intValue(),item.rows.intValue(),null)
        if(shape==null)result.state='not_detected'
        else {
            def polygon=(Polygon)field(shape,'polyGrid')
            result.state='detected';result.rows=field(shape,'nVertical');result.columns=field(shape,'nHorizontal')
            result.centers=(0..<polygon.npoints).collect {i -> [polygon.xpoints[i],polygon.ypoints[i]]}
            result.densities=density.invoke(null,[bp,polygon,item.upstream_core_diameter.doubleValue()] as Object[])
        }
    }catch(Exception error) {result.state='error';result.error_type=error.getClass().getName();result.error=error.getMessage()}
    observations.add(result)
}
def reference=[schema:'qpx.qupath-tma-dearray-reference/1',version:GeneralTools.getVersion(),upstream_revision:contract.upstream_revision,
    synthetic:true,scope:'Binary-mask detector characterization only; not full RGB preprocessing or biological acceptance.',cases:observations]
Files.writeString(output,gson.toJson(reference)+'\n',StandardCharsets.UTF_8)
println('QPX_TMA_DEARRAY_REFERENCE_OK '+observations.size())
