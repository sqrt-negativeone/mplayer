@echo off

:: choose name for the program
set exe_name=mplayer

:: the path to the codebase
set code_dir=%cd%

:: Debug=0, Release=1
set release_mode=0

set compiler_warnings= ^
	/W4 /WX ^
	/wd4100 /wd4127 /wd4189 ^
  /wd4201 ^
	/wd4533 /wd4505 ^
  /wd4459 /wd4471 ^
  /we4061 /we4062

set common_compiler_flags=/FC /Z7 /GR- /EHsc /nologo

:: We are using C++ 20 only for the designated initializers
set compiler_flags=%common_compiler_flags% /std:c++20

set compiler_includes= ^
	/I "%code_dir%/code/"


if %release_mode% EQU 0 ( rem Debug
	set compiler_flags=%compiler_flags% /Od
) else ( rem Release
	set compiler_flags=%compiler_flags% /O2 /MT
)

set compiler_settings=%compiler_includes% %compiler_flags% %compiler_warnings%


:: add libs to link here
set platform_libs= ^
  opengl32.lib user32.lib gdi32.lib

set common_linker_flags=/incremental:no
set platform_linker_flags=%common_linker_flags% /LIBPATH:"%code_dir%/lib/win32"
set platform_linker_settings=%platform_libs% %platform_linker_flags%

set app_linker_settings=%common_linker_flags%

if not exist bld mkdir bld
pushd bld

del *.ilk > NUL 2> NUL
set time_stamp=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%

call cl %compiler_settings% "%code_dir%\code\win32_mplayer_main.cpp" /link %platform_linker_settings% /OUT:%exe_name%.exe

del *.obj > NUL 2> NUL
popd

