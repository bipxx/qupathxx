// Small class-color oracle only; no project, image server or pixel IO.
import java.nio.file.Files
import java.nio.file.Path
import qupath.lib.objects.classes.PathClass
import qupath.lib.io.GsonTools

def rows=[]
def add={ pc,parent -> rows.add([path:pc.toList(),parent_argb:parent?.getColor(),argb:pc.getColor()]) }
for(def name:['Positive','Negative','1+','2+','3+','Unseen α','Class 🚀','é','e\u0301',args[1]])add(PathClass.getInstance(name),null)
def tumor=PathClass.getInstance('Tumor',0xc80000);tumor.setColor(0xc80000)
def other=PathClass.getInstance('Other base',0x204080);other.setColor(0x204080)
def branch=PathClass.getInstance(other,'Branch',0x102030);branch.setColor(0x102030)
def alpha=PathClass.getInstance('Alpha base',(int)0x80204080L);alpha.setColor((int)0x80204080L)
for(def parent:[tumor,other,branch,alpha]) {
    for(def name:['1+','2+','3+','Positive','Negative','Unseen child','Child 🚀'])add(PathClass.getInstance(parent,name,null),parent)
}
Files.writeString(Path.of(args[0]).resolve('default-colors.json'),GsonTools.getInstance(true).newBuilder().serializeNulls().create().toJson([dark_input:args[1],cases:rows])+'\n')
