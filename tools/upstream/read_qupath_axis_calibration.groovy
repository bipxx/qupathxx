// Read exact saved metadata with no project/source-builder substitution.
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools
import qupath.lib.images.servers.ImageServerBuilder
import com.google.gson.JsonParser

def source=Path.of(args[0]);def output=Path.of(args[1])
if(Files.exists(output))throw new IllegalArgumentException('Reference output exists')
def manifest=JsonParser.parseString(Files.readString(source)).getAsJsonObject();def rows=[]
for(def entry:manifest.getAsJsonArray('images')) {
    def id=entry.getAsJsonObject().get('entryID').getAsString()
    def path=source.getParent().resolve('data').resolve(id).resolve('data.qpdata')
    def data=PathIO.readImageData(path,(ImageServerBuilder.ServerBuilder)null)
    try {
        def metadata=data.getServerMetadata();def cal=metadata.getPixelCalibration()
        def factor=[NANOSECONDS:1e-9,MICROSECONDS:1e-6,MILLISECONDS:1e-3,SECONDS:1.0,MINUTES:60.0,HOURS:3600.0,DAYS:86400.0][cal.getTimeUnit().name()]
        rows.add([name:entry.getAsJsonObject().get('imageName').getAsString(),z_spacing_um:Double.toHexString(cal.getZSpacingMicrons()),
            time_unit:cal.getTimeUnit().name(),times:(0..<cal.nTimepoints()).collect{Double.toHexString(cal.getTimepoint(it))},
            seconds:(0..<cal.nTimepoints()).collect{Double.toHexString(cal.getTimepoint(it)*factor)},
            z_planes:metadata.getSizeZ(),timepoints:metadata.getSizeT()])
    } finally {data.close()}
}
Files.writeString(output,GsonTools.getInstance(true).toJson(rows)+'\n')
