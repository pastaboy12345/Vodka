#!/usr/bin/env python3
import struct
import sys
import zipfile


NO_INDEX = 0xFFFFFFFF
TYPE_STRING = 0x03
TYPE_INT_DEC = 0x10


def encoded_length(value):
    if value < 0x80:
        return bytes([value])
    if value < 0x8000:
        return bytes([0x80 | (value >> 8), value & 0xFF])
    raise ValueError("test string is too long")


def chunk_header(chunk_type, header_size, chunk_size):
    return struct.pack("<HHI", chunk_type, header_size, chunk_size)


def string_pool(strings):
    offsets = []
    data = bytearray()

    for value in strings:
        encoded = value.encode("utf-8")
        offsets.append(len(data))
        data += encoded_length(len(value))
        data += encoded_length(len(encoded))
        data += encoded
        data += b"\0"

    while len(data) % 4 != 0:
        data += b"\0"

    header_size = 28
    strings_start = header_size + (len(strings) * 4)
    chunk_size = strings_start + len(data)

    return (
        chunk_header(0x0001, header_size, chunk_size)
        + struct.pack("<IIIII", len(strings), 0, 0x00000100, strings_start, 0)
        + b"".join(struct.pack("<I", offset) for offset in offsets)
        + bytes(data)
    )


def xml_attribute(strings, name, raw_value=NO_INDEX, value_type=TYPE_STRING, value_data=0):
    return struct.pack(
        "<IIIHBBI",
        NO_INDEX,
        strings[name],
        raw_value,
        8,
        0,
        value_type,
        value_data,
    )


def string_attribute(strings, name, value):
    value_index = strings[value]
    return xml_attribute(strings, name, value_index, TYPE_STRING, value_index)


def int_attribute(strings, name, value):
    return xml_attribute(strings, name, NO_INDEX, TYPE_INT_DEC, value)


def start_element(strings, tag, attributes=None):
    attributes = attributes or []
    header_size = 36
    attribute_size = 20
    chunk_size = header_size + (len(attributes) * attribute_size)

    return (
        chunk_header(0x0102, header_size, chunk_size)
        + struct.pack("<II", 0, NO_INDEX)
        + struct.pack("<IIHHHHHH", NO_INDEX, strings[tag], 20, attribute_size, len(attributes), 0, 0, 0)
        + b"".join(attributes)
    )


def end_element(strings, tag):
    return (
        chunk_header(0x0103, 24, 24)
        + struct.pack("<II", 0, NO_INDEX)
        + struct.pack("<II", NO_INDEX, strings[tag])
    )


def build_manifest():
    values = [
        "manifest",
        "package",
        "uses-sdk",
        "minSdkVersion",
        "targetSdkVersion",
        "uses-permission",
        "name",
        "application",
        "activity",
        "intent-filter",
        "action",
        "category",
        "com.example.binary",
        "android.permission.INTERNET",
        ".MainActivity",
        "android.intent.action.MAIN",
        "android.intent.category.LAUNCHER",
    ]
    strings = {value: index for index, value in enumerate(values)}

    chunks = [
        string_pool(values),
        start_element(strings, "manifest", [string_attribute(strings, "package", "com.example.binary")]),
        start_element(strings, "uses-sdk", [
            int_attribute(strings, "minSdkVersion", 23),
            int_attribute(strings, "targetSdkVersion", 31),
        ]),
        end_element(strings, "uses-sdk"),
        start_element(strings, "uses-permission", [
            string_attribute(strings, "name", "android.permission.INTERNET"),
        ]),
        end_element(strings, "uses-permission"),
        start_element(strings, "application"),
        start_element(strings, "activity", [string_attribute(strings, "name", ".MainActivity")]),
        start_element(strings, "intent-filter"),
        start_element(strings, "action", [string_attribute(strings, "name", "android.intent.action.MAIN")]),
        end_element(strings, "action"),
        start_element(strings, "category", [string_attribute(strings, "name", "android.intent.category.LAUNCHER")]),
        end_element(strings, "category"),
        end_element(strings, "intent-filter"),
        end_element(strings, "activity"),
        end_element(strings, "application"),
        end_element(strings, "manifest"),
    ]

    size = 8 + sum(len(chunk) for chunk in chunks)
    return chunk_header(0x0003, 8, size) + b"".join(chunks)


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} OUTPUT.apk", file=sys.stderr)
        return 2

    with zipfile.ZipFile(sys.argv[1], "w", zipfile.ZIP_DEFLATED) as apk:
        apk.writestr("AndroidManifest.xml", build_manifest())

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
