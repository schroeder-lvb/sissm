@echo off
rem Change /MD to /MT for static link

cd src
cl *.c Ws2_32.lib /permissive- /GS /GL /analyze- /W3 /Gy /Zc:wchar_t /Zi /Gm- /O2 /sdl /Zc:inline /fp:precise /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" /errorReport:prompt /WX- /Zc:forScope /Gd /Oy- /Oi /MD /FC /EHsc /nologo /diagnostics:column /D "_CRT_SECURE_NO_WARNINGS" /D "_WINSOCK_DEPRECATED_NO_WARNINGS" /Fesissm.exe
cd ..

