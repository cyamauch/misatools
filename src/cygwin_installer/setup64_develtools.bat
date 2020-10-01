@echo off

rem Install additional packages

C:
chdir C:\cygwin64
setup-x86_64.exe --packages ncurses,wget,zip,gcc-core,gcc-g++,libisl10,libisl-devel,libcloog-isl-devel,libncurses-devel,libreadline-devel,libtiff-devel,make,emacs,emacs-anthy,emacs-el,emacs-w32,emacs-X11 --quiet-mode
setup-x86_64.exe --packages xorg-server,xorg-server-common,xorg-x11-fonts-misc,xorg-x11-fonts-dpi75,xorg-x11-fonts-dpi100,xauth,xterm,xset,xmag,xkill,xinit --quiet-mode


rem Create home directory

C:
chdir C:\cygwin64\bin

bash --login -i -c exit

rem pause
