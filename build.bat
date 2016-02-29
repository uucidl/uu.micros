set MicrosDir=.
set MicrosCLFlags=-I%MicrosDir%/include/ -I%MicrosDir%/libs/glew/include/
set MicrosCLFlags=%MicrosDir%/libs/glew/src/glew.c -DGLEW_STATIC opengl32.lib %MicrosCLFlags%
set MicrosCLFlags=-I%MicrosDir%/libs %MicrosDir%/libs/NT_x64/glfw3.lib gdi32.lib user32.lib shell32.lib %MicrosCLFlags%
set MicrosCLFLags=ole32.lib %MicrosCLFlags%
set MicrosCLFlags=%MicrosDir%/runtime/nt_runtime.cpp %MicrosCLFlags%
set MicrosCLFLags=-MD %MicrosCLFlags%
cl -Fe:demo src\main.cpp %MicrosCLFlags% -nologo -EHsc -FC

