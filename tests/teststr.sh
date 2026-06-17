t=${0%.sh}
X=${t}.xex
T=${t^^}
TC=$T.COM
TL=$TC.lib

rm -f $X
cd ..
make || exit
cd -

../actionc $T.ACT $TC -l  -C -a -w &&
cat $TL $TC >$X &&
atari800 -xl-rev altirra -cart "" $X &&
atari800 -xl-rev 11 -cart "" $X &&
rm -f $X $TL $TC &&
echo OK ||
echo ERROR
