// Pinned QuPath0.7 portable ImageData-property types and refusal/loss boundaries.
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import java.lang.reflect.Array
import qupath.lib.projects.Projects
import qupath.lib.images.ImageData
import qupath.lib.images.servers.ImageServerProvider
import qupath.lib.io.GsonTools
import qupath.lib.objects.PathObjects
import qupath.lib.regions.ImagePlane
import qupath.lib.roi.ROIs

def output=Path.of(args[0]);def directory=output.resolve('project');Files.createDirectory(directory)
def project=Projects.createProject(directory.toFile(),BufferedImage.class);def rows=[]
def portable
portable={v,IdentityHashMap active ->
    if(v==null)return [kind:'null']
    if(v instanceof String || v instanceof Character)return [kind:'text',value:v.toString()]
    if(v instanceof Boolean)return [kind:'bool',value:v]
    if(v instanceof Byte || v instanceof Short || v instanceof Integer || v instanceof Long)return [kind:'integer',value:v.toString()]
    if(v instanceof Float || v instanceof Double)return [kind:'real',value:Double.toHexString(v.doubleValue())]
    if(active.containsKey(v))return [kind:'unsupported',type:'cycle']
    active.put(v,true)
    try {
        if(v instanceof Map) {
            if(v.keySet().any{!(it instanceof String)})return [kind:'unsupported',type:'non-string-map']
            def out=new TreeMap();v.each{k,value -> out.put(k,portable(value,active))};return [kind:'map',value:out]
        }
        if(v instanceof List)return [kind:'list',value:v.collect{portable(it,active)}]
        if(v.getClass().isArray())return [kind:'list',value:(0..<Array.getLength(v)).collect{portable(Array.get(v,it),active)}]
        return [kind:'unsupported',type:v.getClass().getName()]
    } finally {active.remove(v)}
}
['portable-values','unsupported-values','depth-limit','text-limit','count-limit'].each {label ->
    def server=ImageServerProvider.buildServer(output.resolve('source.ome.tif').toString(),BufferedImage.class)
    def entry=project.addImage(server.getBuilder());entry.setImageName(label);def data=new ImageData(server)
    if(label=='portable-values') {
        def shared=new ArrayList([true,null,Long.valueOf('9007199254740993'),'共享'])
        def values=[
            'unicode':'Ω α 🧬','': 'empty key','null':null,'bool':true,
            'byte':Byte.valueOf('-128'),'short':Short.valueOf('-32768'),'int':Integer.MIN_VALUE,
            'min-long':Long.MIN_VALUE,'max-long':Long.MAX_VALUE,'beyond-double':Long.valueOf('9007199254740993'),
            'float':Float.valueOf('1.25'),'double':Double.valueOf('1.2345678901234567'),
            'nan':Double.NaN,'positive-infinity':Double.POSITIVE_INFINITY,'negative-infinity':Double.NEGATIVE_INFINITY,'negative-zero':-0.0d,
            'character':Character.valueOf((char)0x03b1),'array-list':shared,'shared-list':shared,
            'linked-list':new LinkedList(['linked',false]),'arrays-list':Arrays.asList('a','b'),
            'immutable-list':List.of('a','b'),'nullable-list':Collections.unmodifiableList(new ArrayList([null,'x'])),
            'singleton-list':Collections.singletonList('one'),'empty-list':Collections.emptyList(),
            'hash-map':new HashMap(['nested':new ArrayList([false,null,42L])]),
            'linked-map':new LinkedHashMap(['z':1L,'a':'first']),
            'immutable-map':Map.of('a',1L,'b',false),'singleton-map':Collections.singletonMap('only','one'),'empty-map':Collections.emptyMap(),
            'unmodifiable-map':Collections.unmodifiableMap(new HashMap(['a':false])),
            'bytes':([-128,0,127] as byte[]),'shorts':([-32768,32767] as short[]),'ints':([Integer.MIN_VALUE,Integer.MAX_VALUE] as int[]),
            'longs':([Long.MIN_VALUE,Long.MAX_VALUE] as long[]),'floats':([Float.NaN,Float.NEGATIVE_INFINITY,-0.0f,1.25f] as float[]),
            'doubles':([Double.NaN,Double.POSITIVE_INFINITY,-0.0d,1.25d] as double[]),'booleans':([true,false] as boolean[]),
            'chars':(['A' as char,'α' as char] as char[]),'objects':(['array',null,true,7L] as Object[])
        ]
        values.each{k,v -> data.setProperty(k,v)}
    } else if(label=='unsupported-values') {
        def cycle=new ArrayList();cycle.add(cycle)
        data.setProperty('retained','safe');data.setProperty('uuid',UUID.fromString('01234567-89ab-cdef-0123-456789abcdef'))
        data.setProperty('non-string-map',new HashMap([(1L):'bad key']));data.setProperty('unknown-in-list',new ArrayList([1L,UUID.fromString('01234567-89ab-cdef-0123-456789abcdef')]))
        data.setProperty('cycle',cycle)
    } else if(label=='depth-limit') {
        def v='leaf';20.times{v=new ArrayList([v])};data.setProperty('deep',v)
    } else if(label=='text-limit')data.setProperty('large','x'*4097)
    else data.setProperty('many',new ArrayList(Collections.nCopies(4097,1L)))
    data.getHierarchy().getRootObject().addChildObject(PathObjects.createAnnotationObject(ROIs.createRectangleROI(1,2,8,9,ImagePlane.getDefaultPlane())))
    entry.saveImageData(data);def restored=entry.readImageData();def properties=new TreeMap()
    if(label in ['portable-values','unsupported-values'])restored.getProperties().each{k,v -> if(k!='PROJECT_ENTRY_ID')properties.put(k,portable(v,new IdentityHashMap()))}
    rows.add([name:label,properties:properties]);restored.getServer().close();server.close()
}
project.syncChanges();Files.writeString(output.resolve('reference.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson(rows)+'\n')
