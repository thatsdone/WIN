#!/bin/sh
PROGNAME=`basename $0`
WINPRM=win.prm
CHTBL=''
CHTBL_FLAG=0
OUTDIR=''
TD='/tmp'
TRGF=''
SAC2K=''
#
usage(){
  echo "Usage:"
  echo " $PROGNAME [-d outdir] [-p win.prm] [-l] [-t channels.tbl] -f trgfile CHIDs"
  echo "     -l: for SAC2000"
}
#
while getopts 'd:f:hlp:t:v' opt ; do
  case $opt in
  'd') OUTDIR=$OPTARG ;;
  'f') TRGF=$OPTARG ;;
  'l') SAC2K='-l' ;;
  'p') WINPRM=$OPTARG ;;
  't') CHTBL="$OPTARG" ; CHTBL_FLAG=2 ;;
  'h'|'v'|'?') usage ; exit ;;
  esac
done
shift `expr $OPTIND - 1`
case $# in
0) F_CH=2
   CHS=''
   ;;
1) F_CH=1
   CHS=$*
   ;;
*) F_CH=0
   CHS=$*
   ;;
esac
if [ X"$TRGF" = X"" -o ! -f $TRGF ]; then
  echo "Please set trgfile."
  usage
  exit
fi
if [ $F_CH -eq 2 ]; then
  CHS=`wck -c $TRGF | awk '{ORS=" ";print $1;}'`
fi
###
if [ -f $WINPRM ]; then
  if [ $CHTBL_FLAG -ne 2 ]; then
    cat $WINPRM | head -2 | tail -1 | awk '{print $1}' | egrep '\*' > /dev/null
    if [ $? ]; then
      CHTBL_FLAG=0
    else
      CHTBL_FLAG=1
    fi
    CHTBL=`cat $WINPRM | head -2 | tail -1 | awk '{print $1}' | sed -e 's/\*//'`
  fi
  TD=`cat $WINPRM | tail -1 | awk '{print $1}'`
  if [ ! -d $TD ]; then
    TD='/tmp'
  fi
fi
if [ -f ${TRGF}.ch -a $CHTBL_FLAG -eq 0 ]; then
  CHTBL="${TRGF}.ch"
  CHTBL_FLAG=1
fi
if [ X"$CHTBL" != X"" -a -f $CHTBL ]; then
  CHTBL="-t $CHTBL"
else
  CHTBL=''
fi
if [ X"$OUTDIR" != X"" -a -d $OUTDIR ]; then
  OUTDIR="-d $OUTDIR"
else
  echo No such directory: $OUTDIR
  exit
fi
#echo $OUTDIR $CHTBL $TRGF $TD
###
if [ $F_CH -eq 1 ]; then
  cat $TRGF | shmdump -tq - $CHS | wintosac $SAC2K $CHTBL $CHS $OUTDIR
else
  cat $TRGF | shmdump -atq - > $TD/tmp.wintosac.$$
  for i in $CHS ; do
    cat $TD/tmp.wintosac.$$ | wintosac $SAC2K $CHTBL $OUTDIR $i
    echo "$i done"
  done
  rm -f $TD/tmp.wintosac.$$
fi
##
exit

