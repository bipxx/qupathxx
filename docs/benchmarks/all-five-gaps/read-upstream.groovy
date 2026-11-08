import java.nio.file.Path
import java.nio.file.Files
import qupath.lib.io.PathIO
import qupath.lib.io.GsonTools
import qupath.lib.images.servers.ImageServers

def server=ImageServers.buildServer(Path.of(args[1]).toUri(),'--classname','qupath.lib.images.servers.openslide.OpenslideServerBuilder')
try {
 def data=PathIO.readImageData(Path.of(args[0]),server.getBuilder())
 try {
  def rows=data.getHierarchy().getFlattenedObjectList(null).findAll{!it.isRootObject()}.collect {o ->
   def values=[:]
   o.getMeasurementList().getMeasurementNames().each {name ->
    double value=o.getMeasurementList().get(name)
    values[name]=Double.isNaN(value)?'NaN':value==Double.POSITIVE_INFINITY?'Infinity':value==Double.NEGATIVE_INFINITY?'-Infinity':value
   }
   [id:o.getID().toString(),parent:o.getParent().isRootObject()?null:o.getParent().getID().toString(),measurements:values,name:o.getName(),classification:o.getPathClass()?.toString()]
  }
  Files.writeString(Path.of(args[2]),GsonTools.getInstance().toJson(rows))
 } finally {data.getServer().close()}
} finally {server.close()}
