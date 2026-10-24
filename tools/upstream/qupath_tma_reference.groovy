// Public-API interoperability capture against pinned QuPath 0.7.0.
// Synthetic geometry only. No upstream detector/parser implementation copied.
import qupath.lib.images.ImageData
import qupath.lib.images.servers.WrappedBufferedImageServer
import qupath.lib.objects.PathObjects
import qupath.lib.objects.hierarchy.DefaultTMAGrid
import qupath.lib.gui.tma.TMADataIO
import qupath.lib.io.GsonTools
import qupath.lib.common.GeneralTools
import java.awt.image.BufferedImage
import java.nio.file.Files
import java.nio.file.Path
import java.nio.charset.StandardCharsets

def root=Path.of(args[0]);Files.createDirectories(root)
if(Files.exists(root.resolve('reference.json')))throw new IllegalStateException('reference destination already contains a capture')
def image=new BufferedImage(256,192,BufferedImage.TYPE_INT_RGB)
def server=new WrappedBufferedImageServer('qpx-synthetic-tma-interop',image)
def describe={grid -> [columns:grid.getGridWidth(),rows:grid.getGridHeight(),cores:grid.getTMACoreList().collect {core ->
    def roi=core.getROI()
    [name:core.getName(),bounds:[roi.getBoundsX(),roi.getBoundsY(),roi.getBoundsWidth(),roi.getBoundsHeight()],missing:core.isMissing(),case_id:core.getCaseID()?:'']
}]}
try {
    def data=new ImageData(server,ImageData.ImageType.BRIGHTFIELD_H_E)
    def names=['A-1','A-2','A-3','B-1','B-2','B-3']
    def cases=['case-one','case-one','', 'patient \u03b1','comma, value','quoted "case"']
    def cores=(0..5).collect {i ->
        int row=i.intdiv(3)
        int col=i%3
        def core=PathObjects.createTMACoreObject(12.75+col*64.25+row*0.5,18.5+row*72.25+col*0.25,17.25+i*0.5,23.75+i*0.25,i==1 || i==4)
        core.setName(names[i]);if(!cases[i].isEmpty())core.setCaseID(cases[i]);core
    }
    def grid=DefaultTMAGrid.create(cores,3);data.getHierarchy().setTMAGrid(grid)
    def exported=root.resolve('qupath-export.qptma')
    TMADataIO.writeTMAData(exported.toFile(),data,null,-1)
    if(!Files.isRegularFile(exported))throw new IllegalStateException('QuPath export missing')
    def decoded=TMADataIO.importDearrayedTMAData(exported.toFile())
    if(decoded==null || decoded.getTMACoreList().size()!=6)throw new IllegalStateException('QuPath export round trip failed')
    def direct=root.resolve('fractional-import.qptma')
    def text='qpx-synthetic-tma-interop\nNative fractional interchange probe\n\nTMA grid width: 3\nTMA grid height: 2\nCore name\tX\tY\tWidth\tHeight\tPresent\tCase ID\n'
    describe(grid).cores.each {core -> text+=([core.name]+core.bounds.collect {it.toString()}+[(!core.missing).toString(),core.case_id]).join('\t')+'\n'}
    Files.writeString(direct,text,StandardCharsets.UTF_8)
    def imported=TMADataIO.importDearrayedTMAData(direct.toFile())
    if(imported==null || imported.getTMACoreList().size()!=6)throw new IllegalStateException('QuPath fractional import failed')
    def result=[schema:'qpx.qupath-tma-reference/1',qupath_version:GeneralTools.getVersion(),upstream_revision:'04ccfa4fb7d43e9b566393e08e83690b72248d44',
        image_size:[256,192],synthetic:true,created:describe(grid),exported_then_imported:describe(decoded),fractional_imported:describe(imported)]
    if(args.length>1) {
        def nativeGrid=TMADataIO.importDearrayedTMAData(Path.of(args[1]).toFile())
        if(nativeGrid==null)throw new IllegalStateException('native candidate import failed')
        result.native_candidate_imported=describe(nativeGrid)
    }
    Files.writeString(root.resolve('reference.json'),GsonTools.getInstance(true).toJson(result)+'\n',StandardCharsets.UTF_8)
    println('QPX_TMA_REFERENCE_OK 6')
} finally {server.close()}
