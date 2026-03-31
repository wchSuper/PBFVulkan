# -*- coding: utf-8 -*-
import os
import subprocess
import sys

# --- configuration ---
# Path to the glslc executable
GLSLC_EXECUTABLE = "glslc"

# Directory containing the shaders
SHADER_DIR = os.path.dirname(os.path.realpath(__file__))

# Output directory for compiled shaders
OUTPUT_DIR = os.path.join(SHADER_DIR, "spv")

# Shader file extensions to look for
SHADER_EXTENSIONS = [".vert", ".frag", ".comp"]
# --- end configuration ---

def find_glslc():
    """Checks if glslc is in the system's PATH."""
    try:
        subprocess.run([GLSLC_EXECUTABLE, "--version"], capture_output=True, check=True, shell=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Error: glslc not found.")
        print("Please make sure the Vulkan SDK is installed and glslc is in your system's PATH environment variable.")
        return False

def compile_shaders():
    """Compiles all GLSL shaders in the SHADER_DIR."""
    print(f"Searching for shaders in: {SHADER_DIR}")

    # Create output directory if it doesn't exist
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
                
                print(f"Compiling {filename} -> {output_filename}")
                
                command = [
                    GLSLC_EXECUTABLE,
                    file_path,
                    "-o",
                    output_path
                ]
                
                try:
                    # On Windows, it's safer to use shell=True if the executable is in PATH
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