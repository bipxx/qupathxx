from pathlib import Path
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159'
text=(root/'build/qupath-input-stages-301/capture2/probe.groovy').read_text()
text+='''
Files.writeString(root.resolve('runtime.json'),gson.toJson([
    imagej_version:ij.IJ.getVersion(),java_version:System.getProperty('java.version')])+'\\n')
'''
target=work/'tools/upstream/qupath_cell_inputs_probe.groovy';assert not target.exists();target.write_bytes(text.encode())
