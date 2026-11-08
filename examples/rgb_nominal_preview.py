"""Read an explicit nominal RGB region and preview it as a new native document."""
import argparse
import json
import qupathxx as q


def make_preview(document: q.Document, request: q.RgbNominalRequest) -> q.Document:
    return q.make_rgb_nominal_preview(document, request)


def open_source(path: str, reader: str) -> q.ImageServer:
    return q.open_rgb_nominal_source(path, reader)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image')
    parser.add_argument('--reader', choices=['auto', 'ppm', 'tiff', 'openslide'], default='auto')
    parser.add_argument('--x-px', type=int, default=0)
    parser.add_argument('--y-px', type=int, default=0)
    parser.add_argument('--width-px', type=int, required=True)
    parser.add_argument('--height-px', type=int, required=True)
    parser.add_argument('--downsample', type=float, default=1.5)
    parser.add_argument('--no-gui', action='store_true')
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--capture', default='')
    args = parser.parse_args()
    request = q.RgbNominalRequest(args.x_px, args.y_px, args.width_px, args.height_px, args.downsample)
    source = open_source(args.image, args.reader)
    try:
        preview = make_preview(q.Document(source), request)
    finally:
        source.close()
    metadata = preview.server.metadata
    print(json.dumps(dict(algorithm=request.algorithm, source_origin_px=[request.x_px, request.y_px],
        source_extent_px=[request.width_px, request.height_px], nominal_downsample=request.downsample,
        output_shape=[metadata.width, metadata.height],
        preview_calibration_um=[preview.pixel_width_um, preview.pixel_height_um],
        preview_is_new_source=True, original_source_replay_authority=False)))
    if not args.no_gui:
        options = q.GuiOptions()
        options.settle_tiles = True
        q.show(preview, frame_limit=24 if args.smoke else 0, capture_path=args.capture, options=options)


if __name__ == '__main__':
    main()
