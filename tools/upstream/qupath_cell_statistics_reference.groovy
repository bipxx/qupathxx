import qupath.lib.io.GsonTools
import qupath.lib.analysis.stats.RunningStatistics
import java.nio.file.Files
import java.nio.file.Path

def finite = { double value -> Double.isFinite(value) ? value : null }
def vectors = [empty:[], singleton:[7d], pair:[2d, 4d], classic:[2d, 4d, 4d, 4d, 5d, 5d, 7d, 9d],
               constant:[5d, 5d, 5d], offset:[1000000000000d, 1000000000001d, 1000000000002d, 1000000000003d, 1000000000004d]]
def cases = vectors.collect { name, values ->
    def stats = new RunningStatistics()
    values.each { stats.addValue(it) }
    [name:name, values:values, count:values.size(), sum:finite(stats.getSum()), mean:finite(stats.getMean()),
     minimum:finite(stats.getMin()), maximum:finite(stats.getMax()), sample_standard_deviation:finite(stats.getStdDev())]
}
def output = Path.of(args[0]); Files.createDirectories(output.getParent())
Files.writeString(output, GsonTools.getInstance(true).toJson([reference:'QuPath 0.7.0 RunningStatistics', cases:cases]) + '\n')
println 'Cell statistics reference: ' + output
