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

# 只遍历当前目录的文件，不递归遍历子目录
current_dir = os.getcwd()
for fileName in os.listdir(current_dir):
    filepath = os.path.join(current_dir, fileName)
    if os.path.isfile(filepath):
        files.append(filepath)


shaders = [".vert", ".frag", ".comp", ".tese", ".tesc", ".geom", ".rgen", ".rchit", ".rmiss", ".rahit"]
shaderFiles = []
glslangPath = FindGlslang()

for file in files:
	_, ext = os.path.splitext(file)
	ext = ext.lower()
	if ext in shaders:
		shaderFiles.append(file.replace("\\", "/"))
	pass

for shader in shaderFiles:
	shader_split_list = shader.rsplit('/', maxsplit=3)
	out_shader_name = f"{shader_split_list[-1]}"
	complete_shader = f"{shader_split_list[0]}/spv/{out_shader_name}"
	# -V: Vulkan SPIR-V, -I..: include path, -g: keep debug info
	cmd = f'{glslangPath} -V -g -I.. "{shader}" -o "{complete_shader}.spv"'
	# print(f"Compiling: {out_shader_name}")
	result = os.system(cmd)
	if result != 0:
		print(f"ERROR: Failed to compile {out_shader_name}")
	pass