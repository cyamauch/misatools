@echo off

C:
chdir C:\cygwin64\bin

bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin rm -f setup64_misatools.sh'
bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin wget http://www.obs.jp/photocon/packages/setup64_misatools.sh'
bash -c 'cd ~ ; PATH=/usr/local/bin:/usr/bin sh setup64_misatools.sh'

pause
