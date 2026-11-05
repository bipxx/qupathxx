// QuPath 0.7.0 exchange/2: args are project, optional auto/native/geojson mode, optional output directory.
// Native entries bind existing saved data; geometry and state have one authority.
// Writes a new exchange directory; existing output is never replaced.
import java.io.DataInputStream
import java.io.BufferedInputStream
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import java.security.MessageDigest
import qupath.lib.io.GsonTools
import qupath.lib.io.PathIO
import qupath.lib.images.servers.ImageServerBuilder
import com.google.gson.JsonParser
import com.google.gson.JsonObject

def source=Path.of(args[0]).toAbsolutePath().normalize()
def mode='auto'
if(args.length>1)mode=args[1]
if(!(mode in ['auto','native','geojson']))throw new IllegalArgumentException('Mode must be auto, native or geojson')
def output=source.getParent().resolve('qupathxx-exchange')
if(args.length>2)output=Path.of(args[2]).toAbsolutePath().normalize()
if(Files.exists(output)) throw new IllegalArgumentException('Exchange directory already exists: '+output)
def hash={ path ->
    def digest=MessageDigest.getInstance('SHA-256')
    Files.newInputStream(path).withCloseable { stream ->
        byte[] buffer=new byte[1048576];int n
        while((n=stream.read(buffer))!=-1) digest.update(buffer,0,n)
    }
    HexFormat.of().formatHex(digest.digest())
}
// Data-only, bounded producer probe. Version keys are ASCII; this probe does not
// retain/convert server values. The native consumer validates the complete stream,
// including modified UTF-8 and supported record/geometry/state representations.
def nativeHeader={ path ->
    if(Files.size(path)>256L*1024*1024)return false
    try {
        return new DataInputStream(new BufferedInputStream(Files.newInputStream(path))).withCloseable { input ->
            if(input.readUnsignedShort()!=0xaced || input.readUnsignedShort()!=5)return false
            def token=input.readUnsignedByte();int block=-1
            if(token==0x77)block=input.readUnsignedByte()
            else if(token==0x7a)block=input.readInt()
            if(block!=21 || input.readUTF()!='Data file version 3')return false
            token=input.readUnsignedByte();long length=-1
            if(token==0x74)length=input.readUnsignedShort()
            else if(token==0x7c)length=input.readLong()
            if(length<0 || length>16L*1024*1024)return false
            def bytes=input.readNBytes((int)length);if(bytes.length!=length)return false
            def metadata=JsonParser.parseString(new String(bytes,StandardCharsets.UTF_8)).getAsJsonObject()
            def version=metadata.get('dataVersion');def producer=metadata.get('qupathVersion')
            return version!=null && version.isJsonPrimitive() && version.getAsJsonPrimitive().isNumber() && version.getAsBigDecimal().compareTo(BigDecimal.valueOf(3))==0 &&
                producer!=null && producer.isJsonPrimitive() && producer.getAsJsonPrimitive().isString() && producer.getAsString()=='0.7.0'
        }
    } catch(IOException | RuntimeException ignored) {return false}
}

def sourceHash=hash(source)
def manifest=JsonParser.parseString(Files.readString(source)).getAsJsonObject()
if(manifest.get('version').getAsString()!='0.7.0') throw new IllegalArgumentException('Requires QuPath 0.7.0 project')
def providers=['qupath.lib.images.servers.bioformats.BioFormatsServerBuilder',
               'qupath.lib.images.servers.imageio.ImageIoImageServerBuilder',
               'qupath.ext.openslide.OpenSlideServerBuilder',
               'qupath.lib.images.servers.openslide.OpenSlideServerBuilder']
for(def image:manifest.getAsJsonArray('images')) {
    def builder=image.getAsJsonObject().getAsJsonObject('serverBuilder')
    if(builder.get('builderType').getAsString()!='uri' || !providers.contains(builder.get('providerClassName').getAsString()))
        throw new IllegalArgumentException('Only eligible URI image builders may be exported')
    if(builder.keySet().any { !(it in ['builderType','providerClassName','uri','args','metadata']) })
        throw new IllegalArgumentException('Unsupported image builder field')
}
def files=[:]
['classifiers/classes.json','data/metadata.json'].each { name ->
    def path=source.getParent().resolve(name)
    files[name]=Files.isRegularFile(path)?hash(path):null
}
def temporary=Files.createTempDirectory(output.getParent(),'.qupathxx-exchange-')
def gson=GsonTools.getInstance(true)
def entries=[]
for(def image:manifest.getAsJsonArray('images')) {
    def id=image.getAsJsonObject().get('entryID').getAsString()
    if(!(id==~ /[1-9][0-9]*/)) throw new IllegalArgumentException('Invalid project entry ID')
    def dataPath=source.getParent().resolve('data').resolve(id).resolve('data.qpdata')
    if(!Files.isRegularFile(dataPath)) continue
    def before=hash(dataPath)
    boolean nativeEntry=false
    if(mode!='geojson')nativeEntry=nativeHeader(dataPath)
    if(mode=='native' && !nativeEntry)throw new IllegalArgumentException('Native mode requires pinned QuPath 0.7.0 data-file version3')
    if(nativeEntry) {
        if(hash(dataPath)!=before)throw new IllegalStateException('Image data changed during export')
        entries.add([entry_id:id,qpdata_sha256:before,representation:'native-saved-data'])
        continue
    }
    // No fallback to backups/empty objects when the actual data file is corrupt.
    def builder=gson.fromJson(image.getAsJsonObject().get('serverBuilder'),ImageServerBuilder.ServerBuilder.class)
    def data=PathIO.readImageData(dataPath,builder)
    try {
        if(data.getHierarchy().getTMAGrid()!=null)throw new IllegalArgumentException('TMA grids require native-saved-data representation')
        def hierarchy=JsonParser.parseString(gson.toJson(data.getHierarchy()))
        def collection=new JsonObject();collection.addProperty('type','FeatureCollection')
        collection.add('features',hierarchy.getAsJsonObject('root').getAsJsonObject('properties').getAsJsonArray('childObjects'))
        def file=id+'.geojson'
        Files.writeString(temporary.resolve(file),gson.toJson(collection)+'\n',StandardCharsets.UTF_8)
        def metadata=data.getServerMetadata()
        def calibration=metadata.getPixelCalibration()
        def finite={ value -> Double.isFinite(value) && value>0 ? value : null }
        def issues=[]
        def root=data.getHierarchy().getRootObject()
        if(root.getMetadata().keySet().any {it!='Object ID'} || !root.getMeasurementList().asMap().isEmpty())
            issues.add('Historical image facts are not migrated by GeoJSON representation')
        if(data.getImageType().name()!='UNSET') issues.add('Image type/analysis defaults are not migrated')
        if(data.getColorDeconvolutionStains()!=null) issues.add('Image stain defaults are not migrated')
        if(data.getProperties().keySet().any {it!='PROJECT_ENTRY_ID'}) issues.add('ImageData properties are not migrated')
        if(metadata.getSizeZ()>1 || metadata.getSizeT()>1)
            issues.add('Source Z/time calibration is retained; saved ImageData calibration overrides are not applied')
        if(hash(dataPath)!=before) throw new IllegalStateException('Image data changed during export')
        entries.add([entry_id:id,qpdata_sha256:before,representation:'geojson',file:file,sha256:hash(temporary.resolve(file)),
                     pixel_width_um:finite(calibration.getPixelWidthMicrons()),pixel_height_um:finite(calibration.getPixelHeightMicrons()),issues:issues])
    } finally {data.close()}
}
if(hash(source)!=sourceHash) throw new IllegalStateException('Project changed during export')
files.each { name,before ->
    def path=source.getParent().resolve(name)
    if((Files.isRegularFile(path)?hash(path):null)!=before) throw new IllegalStateException('Project metadata changed during export')
}
def index=[schema:'qpx.qupath-project-exchange/2',qupath_version:'0.7.0',project_sha256:sourceHash,files:files,images:entries]
def indexGson=gson.newBuilder().serializeNulls().create()
Files.writeString(temporary.resolve('index.json'),indexGson.toJson(index)+'\n',StandardCharsets.UTF_8)
Files.move(temporary,output)
println('Exported '+entries.size()+' explicit image representations to '+output)
