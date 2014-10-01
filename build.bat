@ECHO OFF

CLS

ECHO.
ECHO BUILDING greed.js ...

ECHO Removing folders
RD /Q /S out\
MD out\

RD /Q /S dist\
MD dist\

COPY dist-cursesjs\*.bmp dist\

ECHO.
ECHO building binaries ...
CMD /C emcc -Oz -DSCOREFILE=\"/usr/games/lib/greed.hs\" -DRELEASE=\"1.0js\" greed.c -o out\greed.bc -I dist-cursesjs\

ECHO.
ECHO building application ...
CD dist\
CMD /C emcc -s ASYNCIFY=1 --emrun -O3 ..\out\greed.bc ..\dist-cursesjs\libcurses.o -o greedjs.html --preload-file pdcfont.bmp --preload-file pdcicon.bmp --shell-file ../template.html
CD ..

ECHO.
ECHO FINISHED