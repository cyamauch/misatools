#!/bin/sh

PKGURL=http://www.obs.jp/photocon/packages/
SLLIB=sllib-1.4.5c
EGGX=eggx-0.93r5
MISATOOLS=misatools-0.31

S0=`md5sum /etc/defaults/etc/skel/.bashrc | cut -d ' ' -f 1`
if [ -f .bashrc ]; then
  S1=`md5sum .bashrc | cut -d ' ' -f 1`
else
  S1=$S0
fi
if [ "$S0" = "$S1" ]; then
  cat /etc/defaults/etc/skel/.bashrc | sed -e 's/\(^#[ ]*\)\(alias ls=.*--color=.*$\)/\2/' > .bashrc
fi

if [ ! -f .Xresources ]; then
  echo "XTerm*scrollBar: true" > .Xresources
fi

if [ ! -f .emacs ]; then
  cat > .emacs <<EOF
(add-hook 'c-mode-hook
   '(lambda ()
     (setq c-basic-offset 4)
    ) t)
(add-hook 'c++-mode-hook
   '(lambda ()
     (setq c-basic-offset 4)
    ) t)
(setq scroll-step 1)
(setq scroll-conservatively 4)
(setq scroll-preserve-screen-position t)
EOF
fi

mkdir -p src
cd src

echo " "
echo "###################"
echo "# Setup SLLIB ... #"
echo "###################"
echo " "

if [ ! -f ${SLLIB}.tar.gz ]; then
    wget ${PKGURL}${SLLIB}.tar.gz
fi
if [ ! -d ${SLLIB} ]; then
    tar zxf ${SLLIB}.tar.gz
fi
cd ${SLLIB}
make clean
make
make install64
cd ..

echo " "
echo "##################"
echo "# Setup eggx ... #"
echo "##################"
echo " "

if [ ! -f ${EGGX}.tar.gz ]; then
    wget ${PKGURL}${EGGX}.tar.gz
fi
if [ ! -d ${EGGX} ]; then
    tar zxf ${EGGX}.tar.gz
fi
cd ${EGGX}
make clean
make
make install
cd ..

echo " "
echo "#######################"
echo "# Setup misatools ... #"
echo "#######################"
echo " "

rm -f ${MISATOOLS}.tar.gz
rm -rf ${MISATOOLS}

if [ ! -f ${MISATOOLS}.tar.gz ]; then
    wget ${PKGURL}${MISATOOLS}.tar.gz
fi
if [ ! -d ${MISATOOLS} ]; then
    tar zxf ${MISATOOLS}.tar.gz
fi
cd ${MISATOOLS}
make clean
make
make install
cd dcraw
make clean
make
make install
cd ..
cd ..

echo " "
echo "##########################"
echo "# List of /usr/local/bin #"
echo "##########################"
ls -F /usr/local/bin
echo " "
echo "####################"
echo "# CONGRATULATIONS! #"
echo "####################"
echo " "
