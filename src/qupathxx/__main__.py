import argparse

from pathlib import Path

from . import Document, Image, open_slide, show


def main():
    parser = argparse.ArgumentParser(description="qupathxx Dear ImGui viewer")
    parser.add_argument("image", nargs="?", help="OpenSlide-supported slide or binary P6 PPM; omit for demo")
    args = parser.parse_args()
    source = Image.demo() if not args.image else (
        Image.read_ppm(args.image) if Path(args.image).suffix.lower() == ".ppm" else open_slide(args.image)
    )
    document = Document(source)
    show(document)


if __name__ == "__main__":
    main()
