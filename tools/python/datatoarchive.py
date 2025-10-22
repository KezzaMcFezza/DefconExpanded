#!/usr/bin/env python3

import os
import sys
import subprocess
import platform

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    localisation_dir = os.path.join(project_root, "localisation")
    
    # determine which rar executable to use
    if platform.system() == "Windows":
        rar_exe = os.path.join(script_dir, "..", "bin", "rar.exe")
    else:
        rar_exe = os.path.join(script_dir, "..", "bin", "rar.bin")
    os.chdir(localisation_dir)
    
    # build the rar command
    cmd = [rar_exe, "a", "-r", "-s", "main.dat", "data/"]
    
    # run it and show rar output
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()
