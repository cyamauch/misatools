@echo off

C:
chdir C:\cygwin\bin

bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin rm -f setup_misatools.sh'
bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin wget http://www.obs.jp/photocon/packages/setup_misatools.sh'
bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin sh setup_misatools.sh'

rem bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin mkdir -p src'
rem bash -c 'cd ~/src ; export PATH=/usr/local/bin:/usr/bin ; wget http://www.obs.jp/photocon/packages/sllib-1.4.5.tar.gz'
rem bash -c 'cd ~/src ; export PATH=/usr/local/bin:/usr/bin ; tar zxf sllib-1.4.5.tar.gz ; cd sllib-1.4.5 ; make'
rem bash -c 'cd ~/src ; export PATH=/usr/local/bin:/usr/bin ; wget http://www.obs.jp/photocon/packages/eggx-0.93r5.tar.gz'
rem bash -c 'cd ~/src ; export PATH=/usr/local/bin:/usr/bin ; tar zxf eggx-0.93r5.tar.gz ; cd eggx-0.93r5 ; make'
pause

