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
for root, _, file_names in os.walk(current_dir):
    for file_name in file_names:
        filepath = os.path.join(root, file_name)
        if os.path.isfile(filepath):
            files.append(filepath)


shaders = [".vert", ".frag", ".comp", ".tese", ".tesc", ".geom", ".rgen", ".rchit", ".rmiss", ".rahit"]
shaderFiles = []
glslangPath = FindGlslang()

out_dir = os.path.join(os.path.dirname(current_dir), "spv")
os.makedirs(out_dir, exist_ok=True)

for file in files:
    _, ext = os.path.splitext(file)
    ext = ext.lower()
    if ext in shaders:
        basename = os.path.basename(file)
        if basename in ("sph_viscosity.comp", "sph_surfacetension.comp"):
            continue
        shaderFiles.append(file.replace("\\", "/"))
    pass

for shader in shaderFiles:
    shader_split_list = shader.rsplit('/', maxsplit=1)
    out_shader_name = f"compshader_{shader_split_list[-1]}"
    complete_shader = os.path.join(out_dir, out_shader_name).replace("\\", "/")
    os.system(glslangPath + " -V " + shader + " -o " + complete_shader + ".spv")
    pass