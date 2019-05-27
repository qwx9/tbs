#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

extern Point pan;
void	select(Point);

char *dbname, *prefix, *mapname = "map1.db";

static Keyboardctl *kctl;
static Mousectl *mctl;

void
lmenu(char **i, void (*fn[])(void))
{
	int n;
	static Menu m;

	m.item = i;
	if((n = menuhit(1, mctl, &m, nil)) >= 0)
		fn[n]();
}

static void
mmenu(void)
{
	enum{
		END,
		SPC,
		QUIT
	};
	static char *i[] = {
		[END] "end turn",
		[SPC] "",
		[QUIT] "quit",
		nil
	};
	Menu m = {
		.item = i
	};

	if(gameover)
		m.item += 2;
	switch(menuhit(2, mctl, &m, nil)){
	case END: if(!gameover){ endturn(); break; }	/* wet floor */
	case QUIT: threadexitsall(nil);
	}
}

char *
estrdup(char *s)
{
	if((s = strdup(s)) == nil)
		sysfatal("estrdup: %r");
	setmalloctag(s, getcallerpc(&s));
	return s;
}

void *
emalloc(ulong n)
{
	void *p;

	if((p = mallocz(n, 1)) == nil)
		sysfatal("emalloc: %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void *
erealloc(void *p, ulong n, ulong on)
{
	void *q;

	q = emalloc(n);
	if(p != nil && on > 0)
		memmove(q, p, on);
	free(p);
	return q;
}

static void
usage(void)
{
	fprint(2, "usage: %s [-d db] [-m map]\n", argv0);
	threadexits("usage");
}

void
threadmain(int argc, char **argv)
{
	Rune r;
	Mouse m, om;

	ARGBEGIN{
	case 'd': dbname = EARGF(usage()); break;
	case 'm': mapname = EARGF(usage()); break;
	default: usage();
	}ARGEND
	if(dbname == nil)
		dbname = smprint("%s.db", progname);
	if(prefix == nil)
		prefix = smprint("/sys/games/lib/%s", progname);
	srand(time(nil));
	init();
	if(initdraw(nil, nil, progname) < 0)
		sysfatal("initdraw: %r");
	if((kctl = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	initimages();
	resetdraw();
	Alt a[] = {
		{mctl->c, &m, CHANRCV},
		{mctl->resizec, nil, CHANRCV},
		{kctl->c, &r, CHANRCV},
		{nil, nil, CHANEND}
	};
	for(;;){
		switch(alt(a)){
		default:
			sysfatal("alt: %r");
		case 0:
			if((m.buttons & 1) == 1 && (om.buttons & 1) == 0)
				select(m.xy);
			if(m.buttons & 2)
				mmenu();
			if(m.buttons & 4)
				dopan(m.xy.x - om.xy.x, m.xy.y - om.xy.y);
			om = m;
			break;
		case 1:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			resetdraw();
			break;
		case 2:
			switch(r){
			case '+':
			case '=':
				if(scale < 16){
					scale++;
					resetdraw();
				}
				break;
			case '-':
				if(scale > 1){
					scale--;
					resetdraw();
				}
				break;
			case 'r':
				scale = 1;
				pan = ZP;
				resetdraw();
				break;
			case Kesc:
				selected = nil;
				redraw(0);
				break;
			}
			break;
		}
	}
}
