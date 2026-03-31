# -*- coding: utf-8 -*-
import os
import subprocess
import sys

GLSLC_EXECUTABLE = "glslc"
SHADER_DIR = os.path.dirname(os.path.realpath(__file__))
OUTPUT_DIR = os.path.join(SHADER_DIR, "spv")
SHADER_EXTENSIONS = [".vert", ".frag", ".comp"]

def find_glslc():
    try:
        subprocess.run([GLSLC_EXECUTABLE, "--version"], capture_output=True, check=True, shell=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Error: glslc not found.")
        print("Please make sure the Vulkan SDK is installed and glslc is in your system's PATH environment variable.")
        return False

def compile_shaders():
    print(f"Searching for shaders in: {SHADER_DIR}")

    if not os.path.exists(OUTPUT_DIR):
        print(f"Creating output directory: {OUTPUT_DIR}")
        os.makedirs(OUTPUT_DIR)

    compiled_count = 0
    error_count = 0

    for filename in os.listdir(SHADER_DIR):
        file_path = os.path.join(SHADER_DIR, filename)
        if os.path.isfile(file_path):
            file_ext = os.path.splitext(filename)[1]
            if file_ext in SHADER_EXTENSIONS:
                output_filename = f"{filename}.spv"
                output_path = os.path.join(OUTPUT_DIR, output_filename)

                command = [
                    GLSLC_EXECUTABLE,
                    file_path,
                    "-o",
                    output_path
                ]

                try:
                    result = subprocess.run(command, capture_output=True, text=True, check=True, shell=True)
                    if result.stdout:
                        print(result.stdout)
                    if result.stderr:
                        print(result.stderr, file=sys.stderr)
                    compiled_count += 1
                except subprocess.CalledProcessError as e:
                    print(f"Error compiling {filename}:", file=sys.stderr)
                    print(e.stdout, file=sys.stdout)
                    print(e.stderr, file=sys.stderr)
                    error_count += 1

    print("\n--- Compilation Summary ---")
    print(f"Successfully compiled: {compiled_count} shaders")
    print(f"Failed to compile: {error_count} shaders")
    if error_count > 0:
        return False
    return True

if __name__ == "__main__":
    if not find_glslc():
        sys.exit(1)

    if compile_shaders():
        print("\nShader compilation finished successfully.")
    else:
        print("\nShader compilation finished with errors.", file=sys.stderr)
        sys.exit(1)
