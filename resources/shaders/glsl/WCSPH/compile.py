# coding: utf-8

import os
import sys


def IsExe(path):
    return os.path.isfile(path) and os.access(path, os.X_OK)


def FindGlslang():
    exeName = "glslangvalidator"
    if os.name == "nt":
        exeName += ".exe"

    for exeDir in os.environ["PATH"].split(os.pathsep):
        fullPath = os.path.join(exeDir, exeName)
        if IsExe(fullPath):
            return fullPath

    sys.exit("Could not find glslangvalidator on PATH.")


files = []

current_dir = os.path.dirname(os.path.abspath(__file__))
for fileName in os.listdir(current_dir):
    filepath = os.path.join(current_dir, fileName)
    if os.path.isfile(filepath):
        files.append(filepath)

glsl_dir = os.path.dirname(current_dir)
spv_dir = os.path.join(os.path.dirname(glsl_dir), "spv")

os.makedirs(spv_dir, exist_ok=True)


shaders = [".comp"]
shaderFiles = []
glslangPath = FindGlslang()

for file in files:
    _, ext = os.path.splitext(file)
    ext = ext.lower()
    if ext in shaders:
        basename = os.path.basename(file)
        if basename in ("sph_viscosity.comp", "sph_surfacetension.comp"):
            continue
        shaderFiles.append(file.replace("\\", "/"))


for shader in shaderFiles:
    out_shader_name = f"compshader_{os.path.basename(shader)}"
    complete_shader = os.path.join(spv_dir, out_shader_name).replace("\\", "/")
    cmd = f'{glslangPath} -V -g -I.. "{shader}" -o "{complete_shader}.spv"'
    result = os.system(cmd)
    if result != 0:
        print(f"ERROR: Failed to compile {out_shader_name}")
        sys.exit(result)
