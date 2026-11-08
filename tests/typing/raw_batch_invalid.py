import qupathxx as q


def invalid(rgb: q.PixelClassifier) -> None:
    options = q.ProjectBatchOptions()
    options.raw_pixel_model = rgb
    options.raw_pixel_x = (0, 8, 7, 2)
    options.raw_pixels = q.PixelStreamOptions()
