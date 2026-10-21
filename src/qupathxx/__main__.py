import argparse

from pathlib import Path

from . import Document, Image, Project, GuiOptions, has_project, open_slide, show


def main():
    parser = argparse.ArgumentParser(description="qupathxx Dear ImGui viewer")
    parser.add_argument("image", nargs="?", help="OpenSlide-supported slide or binary P6 PPM; omit for demo")
    opening = parser.add_mutually_exclusive_group()
    opening.add_argument("--project", help="Open a native SQLite project")
    opening.add_argument("--recover", help="Restore unsaved work from a recovery snapshot")
    parser.add_argument("--image-id", type=int, help="Select a stored project image ID")
    parser.add_argument("--smoke", action="store_true", help="Render 90 frames and verify image upload")
    args = parser.parse_args()
    def launch(document, options):
        report = show(document, frame_limit=90 if args.smoke else 0, options=options)
        if args.smoke and not report.uploaded_tiles:
            raise RuntimeError("smoke run uploaded no image tiles")
    if args.project or args.recover:
        if args.image:
            parser.error("choose an image, --project or --recover")
        project = Project.recover(args.recover) if args.recover else Project.open(args.project)
        if args.image_id is not None:
            project.active_image = args.image_id
        if not project.active_image:
            parser.error("project has no active image")
        options = GuiOptions()
        options.project = project
        launch(project.document(project.active_image), options)
        return
    if args.image_id is not None:
        parser.error("--image-id requires --project or --recover")
    source = Image.demo() if not args.image else (
        Image.read_ppm(args.image) if Path(args.image).suffix.lower() == ".ppm" else open_slide(args.image)
    )
    document = Document(source)
    options = GuiOptions()
    if has_project:
        project = Project()
        project.add_document(document, source_path=args.image or "")
        options.project = project
    launch(document, options)


if __name__ == "__main__":
    main()
