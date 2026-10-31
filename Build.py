"""Build on Windows x64 with Python 3 and a compatible GCC/Clang toolchain."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent
src=root/'src'
out=root/'build/EO_AntiLag.dll'
out.parent.mkdir(exist_ok=True)
subprocess.run(['gcc','-shared','-O2','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-fms-extensions','-Wl,--no-insert-timestamp','-o',str(out),str(src/'anti_lag.c'),str(src/'entry_hook.c'),'-lkernel32','-luser32'],check=True)
print(out)
