import ome.units.UNITS
import ome.units.quantity.Length
import com.google.gson.GsonBuilder

def names = ['INCH','THOU','LINE','FOOT','YARD','MILE','ASTRONOMICALUNIT','LIGHTYEAR','PARSEC','POINT']
def values = names.collect { name ->
    def unit = UNITS.getField(name).get(null)
    [name:name, symbol:unit.getSymbol(), micrometres:new Length(1.0,unit).value(UNITS.MICROMETRE).doubleValue()]
}
new File(args[0], 'reference.json').text = new GsonBuilder().setPrettyPrinting().create().toJson(values) + '\n'
