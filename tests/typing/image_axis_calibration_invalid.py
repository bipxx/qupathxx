from qupathxx import Document, Image, ImageAxisCalibration
value: ImageAxisCalibration="wrong"
ImageAxisCalibration().z_spacing_um=2.0
Document(Image.demo()).set_image_axis_calibration("wrong")
