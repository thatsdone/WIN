#! /bin/sh
#
# $Id: ck_wdisk.sh,v 1.1 2002/05/03 10:49:48 uehira Exp $
#

rawdir=/dat/raw
chfile=/dat/etc/channels.chk
ckcmd=/dat/bin/src/wck_wdisk
address=XXX@hoge.com

$ckcmd $chfile $rawdir/`date '+%y%m%d%H.00'` > /tmp/mail.txt.$$
if [ `grep packet /tmp/mail.txt.$$ | wc -l` -eq 0 ]
then
    cat /tmp/mail.txt.$$ | mail -s `date '+%y%m%d%H.00'` $address
fi
rm /tmp/mail.txt.$$
