import argparse
import sys

from . import Document, Image, Project, GuiOptions, has_project, show, default_workspace_layout_path


def main():
    parser = argparse.ArgumentParser(description="qupathxx Dear ImGui viewer")
    parser.add_argument("image", nargs="?", help="OpenSlide-supported slide or binary P6 PPM; omit for demo")
    opening = parser.add_mutually_exclusive_group()
    opening.add_argument("--project", help="Open a native SQLite project")
    opening.add_argument("--recover", help="Restore unsaved work from a recovery snapshot")
    opening.add_argument("--empty", action="store_true", help="Start an empty project workspace")
    parser.add_argument("--recovery-base", help="Associate --recover with a moved primary of the same identity")
    parser.add_argument("--image-id", type=int, help="Select a stored project image ID")
    parser.add_argument("--smoke", action="store_true", help="Render 90 frames after opening and verify image upload")
    parser.add_argument("--compact", action="store_true", help="Use the compact single-inspector workspace")
    parser.add_argument("--layout", help="Separate .ini file for workspace layout preferences")
    parser.add_argument("--reset-layout", action="store_true", help="Restore the default dock layout")
    args = parser.parse_args()
    if args.compact and (args.layout or args.reset_layout):
        parser.error("--layout and --reset-layout require the docked workspace")
    if args.recovery_base and not args.recover:
        parser.error("--recovery-base requires --recover")
    def launch(document, options):
        options.docked_workspace = not args.compact
        options.reset_layout = args.reset_layout
        options.layout_path = args.layout or (default_workspace_layout_path() if not args.smoke and not args.compact else "")
        report = show(document, frame_limit=90 if args.smoke else 0, options=options)
        if report.layout_error:
            print("Workspace layout: "+report.layout_error, file=sys.stderr)
        if args.smoke and options.startup_kind and not report.startup_succeeded:
            raise RuntimeError(report.project_error or "initial opening cancelled")
        if args.smoke and (not options.project or options.project.active_image) and not report.uploaded_tiles:
            raise RuntimeError("smoke run uploaded no image tiles")
    if args.project or args.recover or args.empty:
        if args.image:
            parser.error("choose an image, --project, --recover or --empty")
        if args.empty and args.image_id is not None:
            parser.error("--empty does not accept --image-id")
        project = Project()
        options = GuiOptions()
        options.project = project
        if not args.empty:
            options.startup_kind = "recovery" if args.recover else "project"
            options.startup_path = args.recover or args.project
            if args.image_id is not None:
                if args.image_id <= 0:
                    parser.error("--image-id must be positive")
                options.startup_image_id = args.image_id
        options.recovery_path, options.recovery_base_path = args.recover or "", args.recovery_base or ""
        launch(project, options)
        return
    if args.image_id is not None:
        parser.error("--image-id requires --project or --recover")
    if args.image:
        project, options = Project(), GuiOptions()
        options.project = project
        options.startup_kind, options.startup_path = "image", args.image
        launch(project, options)
        return
    source = Image.demo()
    document = Document(source)
    options = GuiOptions()
    if has_project:
        project = Project()
        project.add_document(document, source_path=args.image or "")
        options.project = project
    launch(document, options)


if __name__ == "__main__":
    main()
