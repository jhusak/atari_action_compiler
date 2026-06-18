# .lst file obtaining:
# Go to JAC!'s ACTION! repository
# compile: mads ACTION-ROM-OSS-16k.asm -l
# rename ACTION-ROM-OSS-16k.lst to action.lst
#
{
grep -w "[\.]proc" action.lst | gawk '{if ($3!=".proc") { if  (strtonum("0x" $2) <0xA000) next; printf "functab[0x%s]=\"%s",$2,$3; for(i=5; i<=NF; i++) printf " %s", $i; print "\";";}}'
echo 'functab[0xB5BE]="lsh1 ;LShift(val, 4)";'
echo 'functab[0xB5C0]="lsh1 ;LShift(val, cnt)";'
} | grep -v spl_ | sort
