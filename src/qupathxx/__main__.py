"""Optional Python entry point for the shared native application."""
import sys
from ._native import run_application


def main():
    raise SystemExit(run_application(sys.argv[1:]))


if __name__ == "__main__":
    main()
