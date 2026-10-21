import argparse

from . import Document, Image, show


def main():
    parser = argparse.ArgumentParser(description="qupathxx Dear ImGui viewer")
    parser.add_argument("image", nargs="?", help="binary P6 PPM image; omit for synthetic demo")
    args = parser.parse_args()
    document = Document(Image.read_ppm(args.image) if args.image else Image.demo())
    show(document)


if __name__ == "__main__":
    main()
