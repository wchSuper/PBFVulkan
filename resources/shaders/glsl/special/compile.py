#!/usr/bin/env python3
import os
import subprocess
import sys
import glob
import shutil
from pathlib import Path

def compile_shader(shader_path, output_path):
    """编译单个着色器文件到SPIR-V格式"""
    try:
        # 创建输出目录（如果不存在）
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        # 调用glslangValidator编译着色器
        result = subprocess.run(
            ["glslangValidator", "-V", shader_path, "-o", output_path],
            capture_output=True,
            text=True,
            check=True
        )
        print(f"编译成功: {shader_path} -> {output_path}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"编译失败: {shader_path}")
        print(f"错误信息: {e.stderr}")
        return False
    except Exception as e:
        print(f"处理文件时出错: {shader_path}")
        print(f"错误信息: {str(e)}")
        return False

def main():
    # 获取当前脚本所在目录
    current_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 确定输入和输出目录
    glsl_special_dir = current_dir  # 当前目录就是 glsl/special
    glsl_dir = os.path.dirname(current_dir)  # 上一级目录是 glsl
    spv_dir = os.path.join(os.path.dirname(glsl_dir), "spv")  # 与glsl同级的spv目录
    spv_special_dir = os.path.join(spv_dir, "special")  # spv/special目录
    
    # 确保输出目录存在
    os.makedirs(spv_special_dir, exist_ok=True)
    
    # 查找所有着色器文件
    shader_extensions = [".vert", ".frag", ".comp", ".geom", ".tesc", ".tese", ".mesh", ".task", ".rgen", ".rchit", ".rmiss"]
    shader_files = []
    
    for ext in shader_extensions:
        shader_files.extend(glob.glob(os.path.join(glsl_special_dir, f"*{ext}")))
    
    if not shader_files:
        print("未找到着色器文件")
        return
    
    # 编译所有着色器文件
    success_count = 0
    failed_count = 0
    
    for shader_path in shader_files:
        # 获取相对路径和文件名
        rel_path = os.path.relpath(shader_path, glsl_special_dir)
        output_path = os.path.join(spv_special_dir, rel_path + ".spv")
        
        # 编译着色器
        if compile_shader(shader_path, output_path):
            success_count += 1
        else:
            failed_count += 1
    
    # 打印编译摘要
    print(f"\n编译完成:")
    print(f"- 成功: {success_count}")
    print(f"- 失败: {failed_count}")
    print(f"- 总计: {success_count + failed_count}")
    print(f"输出目录: {spv_special_dir}")

if __name__ == "__main__":
    main()