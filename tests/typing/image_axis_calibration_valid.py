from qupathxx import Document, Image, ImageAxisCalibration
value=ImageAxisCalibration(z_spacing_um=4.5,timepoints_seconds=[-30.0,0.0,45.0,120.0])
z: float | None=value.z_spacing_um
times: list[float]=value.timepoints_seconds
t: float | None=value.timepoint_seconds(3)
doc=Document(Image.demo())
doc.set_image_axis_calibration(ImageAxisCalibration(4.5,[0.0]))
owned: ImageAxisCalibration | None=doc.image_axis_calibration
position: float | None=doc.z_position_um(0)
time: float | None=doc.timepoint_seconds(0)
doc.set_image_axis_calibration()
