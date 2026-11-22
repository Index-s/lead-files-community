import argparse
import os
import sys

TEXT_EXTENSIONS = {".txt", ".lua", ".quest", ".py", ".cfg", ".msk", ".msa", ".mse"}


def is_utf8(data):
    try:
        data.decode("utf-8")
        return True
    except UnicodeDecodeError:
        return False


def convert_file(path, codepage, dry_run):
    with open(path, "rb") as source:
        data = source.read()

    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]

    if is_utf8(data):
        return "skipped"

    text = data.decode(codepage)

    if text.encode(codepage) != data:
        raise ValueError("%s does not round-trip through %s" % (path, codepage))

    if not dry_run:
        with open(path, "wb") as target:
            target.write(text.encode("utf-8"))

    return "converted"


def iter_text_files(root):
    if os.path.isfile(root):
        yield root
        return

    for directory, _, names in os.walk(root):
        for name in sorted(names):
            if os.path.splitext(name)[1].lower() in TEXT_EXTENSIONS:
                yield os.path.join(directory, name)


def command_convert(arguments):
    counts = {"converted": 0, "skipped": 0}

    for path in iter_text_files(arguments.path):
        result = convert_file(path, arguments.codepage, arguments.dry_run)
        counts[result] += 1

        if result == "converted":
            print(path)

    print("converted %(converted)d, already utf-8 %(skipped)d" % counts)
    return 0


def command_check(arguments):
    failures = []

    for path in iter_text_files(arguments.path):
        with open(path, "rb") as source:
            if not is_utf8(source.read()):
                failures.append(path)

    for path in failures:
        print(path)

    print("not utf-8: %d" % len(failures))
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description="Convert legacy code page text files to UTF-8")
    commands = parser.add_subparsers(dest="command", required=True)

    convert = commands.add_parser("convert")
    convert.add_argument("path")
    convert.add_argument("codepage")
    convert.add_argument("--dry-run", action="store_true")
    convert.set_defaults(run=command_convert)

    check = commands.add_parser("check")
    check.add_argument("path")
    check.set_defaults(run=command_check)

    arguments = parser.parse_args()
    return arguments.run(arguments)


if __name__ == "__main__":
    sys.exit(main())
