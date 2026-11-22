import argparse
import os
import re
import sys

TEXT_EXTENSIONS = {".txt", ".lua", ".quest", ".py", ".cfg", ".msk", ".msa", ".mse"}
COMMENT_MARKERS = {".py": b"#", ".lua": b"--", ".quest": b"--", ".txt": b"//"}
HIGH_RUN = re.compile(rb"[\x80-\xff]+")


def is_utf8(data):
    try:
        data.decode("utf-8")
        return True
    except UnicodeDecodeError:
        return False


def is_korean(data):
    longest = 0

    for run in HIGH_RUN.findall(data):
        if len(set(run)) == 1:
            return False

        try:
            text = run.decode("euc_kr")
        except UnicodeDecodeError:
            return False

        for character in text:
            code = ord(character)

            if not (0xAC00 <= code <= 0xD7A3 or 0x3130 <= code <= 0x318F):
                return False

        longest = max(longest, len(text))

    return longest >= 2


def find_comment(line, marker):
    quote = None
    index = 0

    while index < len(line):
        byte = line[index:index + 1]

        if quote:
            if byte == b"\\":
                index += 1
            elif byte == quote:
                quote = None
        elif byte in (b'"', b"'"):
            quote = byte
        elif line.startswith(marker, index):
            return index

        index += 1

    return -1


def decode_part(data, codepage):
    if not HIGH_RUN.search(data):
        return data.decode("ascii")

    if is_utf8(data):
        return data.decode("utf-8")

    if is_korean(data):
        return data.decode("cp949")

    text = data.decode(codepage)

    if text.encode(codepage) != data:
        raise ValueError("line does not round-trip through %s" % codepage)

    return text


def convert_line(line, codepage, marker, strip_korean_comments):
    if not HIGH_RUN.search(line):
        return line.decode("ascii"), False

    if strip_korean_comments and marker:
        position = find_comment(line, marker)

        if position >= 0 and is_korean(line[position:]):
            code = line[:position].rstrip()

            if not code:
                return None, True

            return decode_part(code, codepage), True

    return decode_part(line, codepage), False


def convert_file(path, codepage, dry_run, strip_korean_comments):
    with open(path, "rb") as source:
        data = source.read()

    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]

    if is_utf8(data):
        return "skipped", 0

    newline = b"\r\n" if b"\r\n" in data else b"\n"
    marker = COMMENT_MARKERS.get(os.path.splitext(path)[1].lower())
    lines = []
    stripped = 0

    for number, line in enumerate(data.split(newline), 1):
        try:
            text, was_stripped = convert_line(line, codepage, marker, strip_korean_comments)
        except (UnicodeDecodeError, ValueError) as error:
            raise ValueError("%s:%d: %s" % (path, number, error))

        stripped += was_stripped

        if text is not None:
            lines.append(text)

    if not dry_run:
        with open(path, "wb") as target:
            target.write(newline.decode("ascii").join(lines).encode("utf-8"))

    return "converted", stripped


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
    stripped_total = 0
    failures = []

    for path in iter_text_files(arguments.path):
        try:
            result, stripped = convert_file(path, arguments.codepage, arguments.dry_run, arguments.strip_korean_comments)
        except ValueError as error:
            failures.append(str(error))
            continue

        counts[result] += 1
        stripped_total += stripped

        if result == "converted":
            print("%s%s" % (path, " (%d korean comments removed)" % stripped if stripped else ""))

    for failure in failures:
        print("FAILED %s" % failure)

    print("converted %d, already utf-8 %d, korean comments removed %d, failed %d"
          % (counts["converted"], counts["skipped"], stripped_total, len(failures)))
    return 1 if failures else 0


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
    convert.add_argument("--strip-korean-comments", action="store_true")
    convert.set_defaults(run=command_convert)

    check = commands.add_parser("check")
    check.add_argument("path")
    check.set_defaults(run=command_check)

    arguments = parser.parse_args()
    return arguments.run(arguments)


if __name__ == "__main__":
    sys.exit(main())
