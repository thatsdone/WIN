# $Id: Makefile,v 1.1 2000/05/17 02:30:15 uehira Exp $

LNMAN = eventsrtape.1 send_mon.1 sendt_mon.1 sendt_raw.1

all : $(LNMAN)

clean :
	rm -f $(LNMAN)

eventsrtape.1 :
	ln -s events.1 $@
send_mon.1 : 
	ln -s send_raw.1 $@
sendt_mon.1 : 
	ln -s send_raw.1 $@
sendt_raw.1 : 
	ln -s send_raw.1 $@
