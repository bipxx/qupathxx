// QuPath 0.7.0: run with one argument, the .qpproj path. Original project is read-only.
// Writes a new sibling qupathxx-exchange directory; existing output is never replaced.
import java.awt.image.BufferedImage
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import java.security.MessageDigest
import qupath.lib.io.GsonTools
import qupath.lib.io.PathIO
import qupath.lib.projects.ProjectIO
import com.google.gson.JsonParser
import com.google.gson.JsonObject

def source=Path.of(args[0]).toAbsolutePath().normalize()
def output=source.getParent().resolve('qupathxx-exchange')
if(Files.exists(output)) throw new IllegalArgumentException('Exchange directory already exists: '+output)
def hash={ path ->
    def digest=MessageDigest.getInstance('SHA-256')
    Files.newInputStream(path).withCloseable { stream ->
        byte[] buffer=new byte[1048576];int n
        while((n=stream.read(buffer))!=-1) digest.update(buffer,0,n)
    }
    HexFormat.of().formatHex(digest.digest())
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
def project=ProjectIO.loadProject(source.toFile(),BufferedImage.class)
def temporary=Files.createTempDirectory(source.getParent(),'.qupathxx-exchange-')
def gson=GsonTools.getInstance(true)
def entries=[]
for(def entry:project.getImageList()) {
    def dataPath=entry.getEntryPath().resolve('data.qpdata')
    if(!Files.isRegularFile(dataPath)) continue
    def before=hash(dataPath)
    // No fallback to backups/empty objects when the actual data file is corrupt.
    def data=PathIO.readImageData(dataPath,entry.getServerBuilder())
    try {
        def hierarchy=JsonParser.parseString(gson.toJson(data.getHierarchy()))
        def collection=new JsonObject();collection.addProperty('type','FeatureCollection')
        collection.add('features',hierarchy.getAsJsonObject('root').getAsJsonObject('properties').getAsJsonArray('childObjects'))
        def file=entry.getID()+'.geojson'
        Files.writeString(temporary.resolve(file),gson.toJson(collection)+'\n',StandardCharsets.UTF_8)
        def metadata=data.getServerMetadata()
        def calibration=metadata.getPixelCalibration()
        def finite={ value -> Double.isFinite(value) && value>0 ? value : null }
        def issues=[]
        if(data.getImageType().name()!='UNSET') issues.add('Image type/analysis defaults are not migrated')
        if(data.getColorDeconvolutionStains()!=null) issues.add('Image stain defaults are not migrated')
        if(data.getProperties().keySet().any {it!='PROJECT_ENTRY_ID'}) issues.add('ImageData properties are not migrated')
        if(metadata.getSizeZ()>1 || metadata.getSizeT()>1)
            issues.add('Source Z/time calibration is retained; saved ImageData calibration overrides are not applied')
        if(hash(dataPath)!=before) throw new IllegalStateException('Image data changed during export')
        entries.add([entry_id:entry.getID(),qpdata_sha256:before,file:file,sha256:hash(temporary.resolve(file)),
                     pixel_width_um:finite(calibration.getPixelWidthMicrons()),pixel_height_um:finite(calibration.getPixelHeightMicrons()),issues:issues])
    } finally {data.close()}
}
if(hash(source)!=sourceHash) throw new IllegalStateException('Project changed during export')
files.each { name,before ->
    def path=source.getParent().resolve(name)
    if((Files.isRegularFile(path)?hash(path):null)!=before) throw new IllegalStateException('Project metadata changed during export')
}
def index=[schema:'qpx.qupath-project-exchange/1',qupath_version:'0.7.0',project_sha256:sourceHash,files:files,images:entries]
Files.writeString(temporary.resolve('index.json'),gson.toJson(index)+'\n',StandardCharsets.UTF_8)
Files.move(temporary,output)
println('Exported '+entries.size()+' image hierarchies to '+output)
