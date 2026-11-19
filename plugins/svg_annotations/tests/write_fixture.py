"""Generate the SVG annotation baseline with svgwrite 1.4.3."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

import svgwrite


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    assert svgwrite.__version__ == "1.4.3"
    drawing = svgwrite.Drawing(str(args.output), size=(4, 3), viewBox="0 0 8 6", profile="full")
    group = drawing.g(id="adjusted", transform="translate(1 1)")
    group.add(drawing.rect(insert=(1, 1), size=(2, 2), id="region", fill="#cc3300"))
    group.add(drawing.polygon(points=[(0, 0), (2, 0), (1, 2)], id="triangle"))
    drawing.add(group)
    drawing.add(drawing.ellipse(center=(5, 3), r=(1, .5), id="ell"))
    drawing.add(drawing.polyline(points=[(1, 4), (3, 5), (6, 4)], id="trace", fill="none", stroke="#0044ff"))
    drawing.add(drawing.path(d="M 0 0 h 8 v 6 h -8 Z M 2 2 H 6 V 4 H 2 Z",
                             id="ring", fill_rule="evenodd"))
    drawing.save()
    root = ET.parse(args.output).getroot()
    assert len([e for e in root.iter() if "id" in e.attrib]) == 6 and root.attrib["viewBox"] == "0 0 8 6"


if __name__ == "__main__":
    main()
