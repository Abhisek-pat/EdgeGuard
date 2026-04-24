from pathlib import Path
import sys
import re

def make_c_identifier(name: str) -> str:
    name = re.sub(r'[^0-9a-zA-Z_]', '_', name)
    if name and name[0].isdigit():
        name = "_" + name
    return name

def main():
    if len(sys.argv) != 3:
        print("Usage: python tflite_to_header.py <input.tflite> <output.h>")
        sys.exit(1)

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    data = input_path.read_bytes()
    var_name = make_c_identifier(input_path.stem)

    with output_path.open("w", encoding="utf-8") as f:
        f.write("#pragma once\n\n")
        f.write("#include <stddef.h>\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"const unsigned char {var_name}[] = {{\n")

        for i, b in enumerate(data):
            if i % 12 == 0:
                f.write("    ")
            f.write(f"0x{b:02x}")
            if i != len(data) - 1:
                f.write(", ")
            if i % 12 == 11:
                f.write("\n")

        if len(data) % 12 != 0:
            f.write("\n")

        f.write("};\n\n")
        f.write(f"const unsigned int {var_name}_len = {len(data)};\n")

    print(f"Wrote {output_path}")
    print(f"Array name: {var_name}")
    print(f"Length: {len(data)} bytes")

if __name__ == "__main__":
    main()