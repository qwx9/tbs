#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "fns.h"

Point p0, pan;
int scale;
int menuon;
void (*selectfn)(void);

static int fbsz, fbh, fbw;
static u32int *fb;
static Image *fbi;
static Rectangle fbr;
static int tlwidth, tlheight;
static u32int *pics[Pend];
static u32int bgcol = 0x00ffff;

void
dopan(int dx, int dy)
{
	Rectangle r;

	r = rectaddpt(fbr, pan);
	if(dx > 0){
		r.max.x = r.min.x + dx;
		draw(screen, r, display->black, nil, ZP);
	}else if(dx < 0){
		r.min.x = r.max.x + dx;
		draw(screen, r, display->black, nil, ZP);
	}
	r = rectaddpt(fbr, pan);
	if(dy > 0){
		r.max.y = r.min.y + dy;
		draw(screen, r, display->black, nil, ZP);
	}else if(dy < 0){
		r.min.y = r.max.y + dy;
		draw(screen, r, display->black, nil, ZP);
	}
	pan.x += dx;
	pan.y += dy;
	redraw(0);
}

void
select(Point m)
{
	int n;
	Rectangle r;

	r = rectaddpt(fbr, pan);
	if(r.max.y > p0.y)
		r.max.y = p0.y;
	if(!ptinrect(m, r))
		return;
	m = subpt(m, r.min);
	n = m.y / (scale * tlheight) * mapwidth + m.x / (scale * tlwidth);
	selected2 = map + n;
	selectfn();
	selected = selected2;
	redraw(0);
}

static void
drawscaled(void)
{
	Rectangle r, r2;
	uchar *p;

	r = rectaddpt(fbr, pan);
	if(r.max.y > p0.y)
		r.max.y = p0.y;
	r2 = r;
	p = (uchar *)fb;
	if(scale == 1){
		loadimage(fbi, fbi->r, p, fbsz);
		draw(screen, r, fbi, nil, ZP);
	}else{
		while(r.min.y < r2.max.y){
			r.max.y = r.min.y + scale;
			p += loadimage(fbi, fbi->r, p, fbsz / fbh);
			draw(screen, r, fbi, nil, ZP);
			r.min.y = r.max.y;
		}
	}
}

static void
drawpic(u32int *pp, u32int **pic)
{
	int n, m, w, x;
	u32int v, *ip;

	if(pic == nil){
		fprint(2, "drawpic: missing pic\n");
		return;
	}
	n = tlheight;
	ip = *pic;
	w = (fbw - tlwidth) * scale;
	while(n-- > 0){
		m = tlwidth;
		while(m-- > 0){
			v = *ip++;
			if(v & 0xff << 24)
				for(x=0; x<scale; x++)
					*pp++ = v;
			else
				pp += scale;
		}
		pp += w;
	}
}

static void
drawextra(void)
{
	Map *m;

	for(m=map; m<mape; m++)
		if(m->u != nil){
			if(m->u->hp < 99)
				drawpicat(m, pics + m->u->hp / 11 + 1);
			if(m->u->done)
				composeat(m, 0x555555);
		}
}

static void
drawmenuselected(void)
{
	int cost;
	char s[256];
	Point sp;
	Unit *u;
	Tunit *tu;

	tu = &saved->t->spawn;
	if(selected == nil || tu->u + (selected - map) >= tu->e)
		return;
	u = tu->u[selected - map];
	if(team[curteam].unique != nil && u == team[curteam].unique->u){
		if(unique != nil)
			return;
		cost = team[curteam].unique->ecost;
	}else
		cost = u->cost;
	sp = p0;
	sp.y += font->height;
	snprint(s, sizeof s, "unit: %s, atk %d±%d, def %d rng %d-%d, mp %d, cost %d", u->name, u->atk, u->Δatk, u->def, u->rmin, u->rmax, u->mp, cost);
	string(screen, sp, display->white, ZP, font, s);
}

static void
drawspawn(void)
{
	Tunit *tu;
	Unit **u;
	Map *m;

	tu = &saved->t->spawn;
	for(u=tu->u, m=map; u<tu->e; u++, m++){
		drawpicat(m, (*u)->pic);
		if(unique != nil && *u == team[curteam].unique->u)
			composeat(m, 0x555555);
	}
}

static void
drawselected(void)
{
	char s[256];
	Point sp;
	Munit *mu;
	Unit *u;

	if(selected == nil)
		return;
	drawpicat(selected, pics + Pcur);
	sp = p0;
	sp.y += font->height;
	snprint(s, sizeof s, "terrain: %s (+%d)", selected->t->name, selected->t->def);
	string(screen, sp, display->white, ZP, font, s);
	if(selected->u == nil)
		return;
	sp.y += font->height;
	mu = selected->u;
	u = mu->u;
	snprint(s, sizeof s, "unit: %s, rank %d, atk %d±%d, def %d, rng %d-%d, mp %d", u->name, mu->xp / 100, mu->eatk, u->Δatk, mu->edef, u->rmin, u->rmax, u->mp);
	string(screen, sp, display->white, ZP, font, s);
	flushimage(display, 1);
}

static void
drawlog(void)
{
	char s[64];

	if(gameover)
		snprint(s, sizeof s, "GAME OVER: team %d wins at turn %d", curteam, turn+1);
	else
		snprint(s, sizeof s, "turn %d, team %d: %d (+%d) gold, %d units %d buildings", turn+1, curteam, team[curteam].money, team[curteam].income, team[curteam].nunit, team[curteam].nbuild);
	string(screen, p0, display->white, ZP, font, s);
}

static void
drawmap(void)
{
	int n, w;
	u32int *pp;
	Map *m;

	pp = fb;
	n = 0;
	w = tlwidth * mapwidth * (tlheight - 1) * scale;
	for(m=map; m<mape; m++){
		drawpic(pp, m->t->pic + m->team);
		if(m->u != nil)
			drawpic(pp, m->u->u->pic + m->u->team);
		if(m->canmove)
			composeat(m, 0xff0f0f);
		if(m->canatk)
			composeat(m, 0xf0ff0f);
		pp += tlwidth * scale;
		if(++n == mapwidth){
			n = 0;
			pp += w;
		}
	}
}

void
composeat(Map *m, u32int c)
{
	int k, n, x, w;
	u32int v, *pp;

	w = fbw * scale * tlheight;
	pp = fb + (m-map) / mapwidth * w
		+ (m-map) % mapwidth * tlwidth * scale;
	n = tlheight;
	w = (fbw - tlwidth) * scale;
	while(n-- > 0){
		x = tlwidth;
		while(x-- > 0){
			v = *pp;
			v = (v & 0xff0000) + (c & 0xff0000) >> 1 & 0xff0000
				| (v & 0xff00) + (c & 0xff00) >> 1 & 0xff00
				| (v & 0xff) + (c & 0xff) >> 1 & 0xff;
			k = scale;
			while(k-- > 0)
				*pp++ = v;
		}
		pp += w;
	}
}

void
drawpicat(Map *m, u32int **pic)
{
	int w;
	u32int *pp;

	w = fbw * scale * tlheight;
	pp = fb + (m-map) / mapwidth * w + (m-map) % mapwidth * tlwidth * scale;
	drawpic(pp, pic);
}

void
redraw(int all)
{
	if(all)
		draw(screen, screen->r, display->black, nil, ZP);
	else
		draw(screen, Rpt(p0, screen->r.max), display->black, nil, ZP);
	if(!menuon){
		drawmap();
		drawselected();
		drawextra();
	}else{
		memset(fb, 0, mapwidth * tlwidth * scale * mapheight * tlheight * sizeof *fb);
		drawspawn();
		drawmenuselected();
	}
	drawlog();
	drawscaled();
	flushimage(display, 1);
}

void
resetdraw(void)
{
	Point p, d;

	if(scale < 1)
		scale = 1;
	else if(scale > 16)
		scale = 16;
	fbw = mapwidth * tlwidth;
	fbh = mapheight * tlheight;
	p = divpt(addpt(screen->r.min, screen->r.max), 2);
	d = Pt(fbw * scale / 2, fbh * scale / 2);
	fbr = Rpt(subpt(p, d), addpt(p, d));
	p0 = Pt(screen->r.min.x + 8, screen->r.max.y - 3 * font->height);
	fbsz = fbw * scale * fbh * sizeof *fb;
	free(fb);
	freeimage(fbi);
	fb = emalloc(fbsz);
	if((fbi = allocimage(display, Rect(0,0,fbw*scale,scale==1 ? fbh : 1), XRGB32, scale>1, 0)) == nil)
		sysfatal("allocimage: %r");
	redraw(1);
}

static Image*
iconv(Image *i)
{
	Image *ni;

	if(i->chan == RGB24)
		return i;
	if((ni = allocimage(display, i->r, RGB24, 0, DNofill)) == nil)
		sysfatal("allocimage: %r");
	draw(ni, ni->r, i, nil, ZP);
	freeimage(i);
	return ni;
}

static int
openimage(char *name)
{
	int fd;
	char *s;

	if((fd = open(name, OREAD)) < 0){
		s = smprint("%s/%s", prefix, name);
		fd = open(s, OREAD);
		free(s);
		if(fd < 0)
			fprint(2, "openimage: %r\n");
	}
	return fd;
}

static u32int *
loadpic(char *name)
{
	int fd, n, m, dx, dy;
	Image *i;
	uchar *b, *s;
	u32int v, *p, *pic;

	if(name == nil || strlen(name) == 0)
		sysfatal("loadpic: invalid name");
	if((fd = openimage(name)) < 0)
		return nil;
	if((i = readimage(display, fd, 0)) == nil)
		sysfatal("readimage: %r");
	close(fd);
	i = iconv(i);
	dx = Dx(i->r);
	dy = Dy(i->r);
	if(tlwidth == 0 || tlheight == 0){
		tlwidth = dx;
		tlheight = dy;
	}else if(tlwidth != dx || tlheight != dy)
		sysfatal("loadpic: unequal tile size %dx%d not %dx%d",
			dx, dy, tlwidth, tlheight);
	n = dx * dy;
	m = n * i->depth / 8;
	b = emalloc(m);
	unloadimage(i, i->r, b, m);
	freeimage(i);
	p = emalloc(n * sizeof *p);
	pic = p;
	s = b;
	while(n-- > 0){
		v = s[2] << 16 | s[1] << 8 | s[0];
		if(v != bgcol)
			v |= 0xff << 24;
		*p++ = v;
		s += 3;
	}
	free(b);
	return pic;
}

static void
initbaseimages(void)
{
	char *tab[] = {
		[Pcur] "cursor.bit",
		[P1] "1.bit",
		[P2] "2.bit",
		[P3] "3.bit",
		[P4] "4.bit",
		[P5] "5.bit",
		[P6] "6.bit",
		[P7] "7.bit",
		[P8] "8.bit",
		[P9] "9.bit",
	};
	int i;

	for(i=0; i<nelem(tab); i++)
		pics[i] = loadpic(tab[i]);
}

void
initimages(void)
{
	int i, n;
	char s[64];
	Terrain *t;
	Unit *u;

	initbaseimages();
	for(t=terrain; t<terrain+nterrain; t++){
		n = t->occupy.u != nil ? nteam+1 : 1;
		t->pic = emalloc(n * sizeof *t->pic);
		if(strcmp(t->name, "void") == 0)
			continue;
		snprint(s, sizeof s, "%s.bit", t->name);
		t->pic[0] = loadpic(s);
		for(i=1; i<n; i++){
			snprint(s, sizeof s, "%s%d.bit", t->name, i);
			t->pic[i] = loadpic(s);
		}
	}
	for(u=unit; u<unit+nunit; u++){
		u->pic = emalloc((nteam+1) * sizeof *u->pic);
		if(strcmp(u->name, "void") == 0)
			continue;
		snprint(s, sizeof s, "%s.bit", u->name);
		u->pic[0] = loadpic(s);
		if(strcmp(u->name, "corpse") == 0)
			continue;
		for(i=1; i<=nteam; i++){
			snprint(s, sizeof s, "%s%d.bit", u->name, i);
			u->pic[i] = loadpic(s);
		}
	}
}
