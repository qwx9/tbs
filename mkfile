</$objtype/mkfile
TARG=\
	tbs1\
	tbs2\
	tchs\

OFILES=\
	db.$O\
	drw.$O\
	game.$O\
	tbs.$O\

HFILES=dat.h fns.h
</sys/src/cmd/mkmany
BIN=$home/bin/$objtype

$O.tbs1: $OFILES tbs1.$O
	$LD -o $target $prereq

$O.tbs2: $OFILES tbs2.$O
	$LD -o $target $prereq

$O.tchs: $OFILES tchs.$O
	$LD -o $target $prereq

sysinstall:V:
	mkdir -p /sys/games/lib/^(tbs1 tbs2 tchs)
	dircp tbs1 /sys/games/lib/tbs1
	dircp tbs2 /sys/games/lib/tbs2
	dircp tchs /sys/games/lib/tchs
