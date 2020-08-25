@echo off

C:
chdir C:\cygwin\bin

bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin rm -f setup_misatools.sh'
bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin wget http://www.obs.jp/photocon/packages/setup_misatools.sh'
bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin sh setup_misatools.sh'

pause
