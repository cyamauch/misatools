@echo off

rem Install additional packages

C:
chdir C:\cygwin
setup-x86.exe --packages ncurses,wget,zip,gcc-core,gcc-g++,libisl10,libisl-devel,libcloog-isl-devel,libncurses-devel,libreadline-devel,libtiff-devel,make,emacs,emacs-anthy,emacs-el,emacs-w32,emacs-X11 --quiet-mode


rem Create home directory

C:
chdir C:\cygwin\bin

bash --login -i -c exit

rem pause