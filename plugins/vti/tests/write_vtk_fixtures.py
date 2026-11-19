"""Generate reference scalar volumes using the independent VTK 9.4.2 writer."""
import argparse
from pathlib import Path

import numpy as np
from vtkmodules.util.numpy_support import numpy_to_vtk, vtk_to_numpy
from vtkmodules.vtkCommonCore import vtkVersion
from vtkmodules.vtkCommonDataModel import vtkImageData
from vtkmodules.vtkImagingCore import vtkRTAnalyticSource
from vtkmodules.vtkIOXML import vtkXMLImageDataReader, vtkXMLImageDataWriter


def write_streamed(output, compressor):
    source = vtkRTAnalyticSource()
    source.SetWholeExtent(0, 2, 0, 1, 0, 4)
    source.Update()
    expected = vtk_to_numpy(source.GetOutput().GetPointData().GetScalars()).copy()
    np.save(output / "expected.npy", expected.reshape(5, 2, 3))
    for mode, raw in (("Ascii", False), ("Binary", False), ("Appended", False), ("Appended", True)):
        path = output / (mode.lower() + ("-raw" if raw else "") + ".vti")
        writer = vtkXMLImageDataWriter()
        writer.SetFileName(str(path))
        writer.SetInputConnection(source.GetOutputPort())
        writer.SetNumberOfPieces(2)
        if compressor:
            getattr(writer, "SetCompressorTypeTo" + compressor)()
            writer.SetBlockSize(16)
        getattr(writer, "SetDataModeTo" + mode)()
        if raw:
            writer.EncodeAppendedDataOff()
        assert writer.Write()
        reader = vtkXMLImageDataReader()
        reader.SetFileName(str(path))
        reader.Update()
        assert np.array_equal(vtk_to_numpy(reader.GetOutput().GetPointData().GetScalars()), expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    compression = parser.add_mutually_exclusive_group()
    compression.add_argument("--compressed", dest="compressor", action="store_const", const="ZLib")
    compression.add_argument("--lz4", dest="compressor", action="store_const", const="LZ4")
    compression.add_argument("--lzma", dest="compressor", action="store_const", const="LZMA")
    parser.add_argument("--raw-appended", action="store_true")
    parser.add_argument("--streamed", action="store_true")
    parser.add_argument("--components", choices=("scalars", "vectors"))
    parser.add_argument("--multiple-arrays", action="store_true")
    parser.add_argument("--shifted-extent", action="store_true")
    args = parser.parse_args()
    assert vtkVersion.GetVTKVersion() == "9.4.2"
    args.output.mkdir()
    if args.streamed:
        if args.compressor == "ZLib" or args.raw_appended or args.components or args.multiple_arrays or args.shifted_extent:
            parser.error("--streamed already generates all four writer layouts")
        write_streamed(args.output, args.compressor)
        return
    image = vtkImageData()
    image.SetDimensions(3, 2, 2)
    image.SetSpacing(0.0005, 0.0015, 0.003)
    if args.shifted_extent:
        image.SetExtent(10, 12, -5, -4, 7, 8)
        image.SetSpacing(0.25, 0.5, 2)
        image.SetOrigin(-2.5, 2.5, -14)
    values = np.arange(12, dtype=np.uint16)
    if args.components:
        values = np.column_stack((values * 257, values + 8192, 60000 - values * 17))
    scalars = numpy_to_vtk(values, deep=True)
    scalars.SetName("Research samples")
    if args.components == "vectors":
        scalars.SetComponentName(0, "Horizontal")
        scalars.SetComponentName(2, "Depth")
        image.GetPointData().SetVectors(scalars)
    else:
        image.GetPointData().SetScalars(scalars)
    if args.multiple_arrays:
        if args.components:
            parser.error("--multiple-arrays supplies its own point array layout")
        auxiliary = values * 9 + 1000
        auxiliary_array = numpy_to_vtk(auxiliary, deep=True)
        auxiliary_array.SetName("Auxiliary")
        image.GetPointData().AddArray(auxiliary_array)
        vectors = np.column_stack((values * 257, values + 8192, 60000 - values * 17))
        vector_array = numpy_to_vtk(vectors, deep=True)
        vector_array.SetName("Displacement")
        image.GetPointData().SetVectors(vector_array)
    modes = ("Binary", "Appended") if args.compressor else ("Ascii", "Binary", "Appended")
    if args.raw_appended:
        modes = ("Appended",)
    for mode in modes:
        for order, header in (("LittleEndian", "UInt32"), ("BigEndian", "UInt64")):
            path = args.output / f"{mode.lower()}-{order}-{header}.vti"
            writer = vtkXMLImageDataWriter()
            writer.SetFileName(str(path))
            writer.SetInputData(image)
            if args.raw_appended:
                writer.EncodeAppendedDataOff()
            if args.compressor:
                getattr(writer, "SetCompressorTypeTo" + args.compressor)()
                writer.SetBlockSize(8 if header == "UInt32" else 16)
            else:
                writer.SetCompressorTypeToNone()
            getattr(writer, "SetDataModeTo" + mode)()
            getattr(writer, "SetByteOrderTo" + order)()
            getattr(writer, "SetHeaderTypeTo" + header)()
            assert writer.Write()
            reader = vtkXMLImageDataReader()
            reader.SetFileName(str(path))
            reader.Update()
            result = reader.GetOutput()
            assert result.GetExtent() == image.GetExtent() and result.GetSpacing() == image.GetSpacing()
            if args.shifted_extent:
                assert result.GetPoint(0) == (0, 0, 0)
                assert result.GetPoint(11) == (0.5, 0.5, 2)
            points = result.GetPointData()
            data = points.GetVectors() if args.components == "vectors" else points.GetScalars()
            assert np.array_equal(vtk_to_numpy(data), values)
            if args.components == "vectors":
                assert data.GetComponentName(0) == "Horizontal" and data.GetComponentName(2) == "Depth"
            if args.multiple_arrays:
                assert np.array_equal(vtk_to_numpy(points.GetArray("Auxiliary")), auxiliary)
                assert np.array_equal(vtk_to_numpy(points.GetVectors()), vectors)


if __name__ == "__main__":
    main()
