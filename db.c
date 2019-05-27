#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include "dat.h"
#include "fns.h"

char **mov;
int nmov;
Terrain *terrain;
int nterrain;
Unit *unit;
int nunit;
Map *map, *mape, *selected, *selected2;
int mapwidth, mapheight;

typedef struct Stackenblochen Stackenblochen;
struct Stackenblochen{
	char *name;
	int v;
	char *s;
	Stackenblochen *b;
	Stackenblochen *l;
	Stackenblochen *r;
};
static Stackenblochen stack0 = {.l = &stack0, .r = &stack0}, *stack = &stack0;

static Stackenblochen *
allocblochen(char *name)
{
	Stackenblochen *b;

	b = emalloc(sizeof *b);
	if(name[0] != 0)
		b->name = estrdup(name);
	return b;
}

static void
freeblochen(Stackenblochen *b)
{
	free(b->name);
	free(b->s);
	free(b);
}

static void
popstackenblochen(Stackenblochen *stack)
{
	Stackenblochen *b, *p, *q;

	for(b=stack->l; b!=stack; b=q){
		for(p=b->r; p!=b; p=q){
			q = p->r;
			freeblochen(p);
		}
		q = b->l;
		freeblochen(b);
	}
	for(b=stack; b->r!=stack; b=b->r)
		;
	b->r = b->r->r;
	freeblochen(stack);
}

static Stackenblochen *
pushstackenblochen(char *name, Stackenblochen *stack)
{
	Stackenblochen *b, *p;

	b = allocblochen(name);
	b->r = b;
	b->l = stack;
	b->v = stack->v++;
	p = stack;
	while(p->l != stack)
		p = p->l;
	p->l = b;
	return b;
}

static Stackenblochen *
stackenblochen(char *name, Stackenblochen *stack)
{
	Stackenblochen *b;

	b = allocblochen(name);
	b->l = b;
	b->r = stack->r;
	stack->r = b;
	return b;
}

static Stackenblochen *
findblochen(char *name, Stackenblochen *stack, int dir)
{
	Stackenblochen *b;

	b = dir ? stack->l : stack->r;
	while(b != stack){
		if(b->name != nil && strcmp(b->name, name) == 0)
			return b;
		b = dir ? b->l : b->r;
	}
	return nil;
}

static void
getblochen(Ndbtuple *t, Stackenblochen *stack)
{
	char *s;
	Stackenblochen *b;

	if((b = findblochen(t->attr, stack, 0)) == nil)
		b = stackenblochen(t->attr, stack);
	b = pushstackenblochen(t->val, b);
	for(t=t->entry; t!=nil; t=t->entry){
		b = stackenblochen(t->attr, b);
		b->v = strtol(t->val, &s, 0);
		b->s = estrdup(t->val);
	}
}

static void
loaddb(char *name, Stackenblochen *stack)
{
	char *s;
	Ndb *db;
	Ndbtuple *t;

	if((db = ndbopen(name)) == nil){
		s = smprint("%s/%s", prefix, name);
		db = ndbopen(s);
		free(s);
		if(db == nil)
			sysfatal("ndbopen: %r");
	}
	while((t = ndbparse(db)) != nil){
		getblochen(t, stack);
		ndbfree(t);
	}
	ndbclose(db);
}

static void
initrules(void)
{
	Stackenblochen *b;

	unitcap = 25;
	if((b = findblochen("initmoney", stack, 0)) != nil){
		initmoney = strtol(b->l->name, nil, 0);
		if(initmoney < 0)
			initmoney = 0;
		popstackenblochen(b);
	}
	if((b = findblochen("unitcap", stack, 0)) != nil){
		unitcap = strtol(b->l->name, nil, 0);
		if(unitcap <= 0)
			unitcap = mapwidth * mapheight / nteam;
		popstackenblochen(b);
	}
	if((b = findblochen("firstturnnoinc", stack, 0)) != nil){
		firstturnnoinc = 1;
		popstackenblochen(b);
	}
	if((b = findblochen("nocorpse", stack, 0)) != nil){
		nocorpse = 1;
		popstackenblochen(b);
	}
}

static void
initmap(void)
{
	int i, l, n;
	Map *m;
	Unit *u;
	Stackenblochen *b, *p, *q;

	if((b = findblochen("mapwidth", stack, 0)) != nil){
		mapwidth = strtol(b->l->name, nil, 0);
		if(mapwidth <= 0)
			mapwidth = 1;
		popstackenblochen(b);
	}
	if((b = findblochen("map", stack, 0)) != nil){
		if(mapwidth <= 0)
			mapwidth = sqrt(b->v);
		mapheight = b->v / mapwidth;
	}else{
		if(mapwidth <= 0)
			mapwidth = 8;
		mapheight = mapwidth;
	}
	n = mapwidth * mapheight;
	map = emalloc(n * sizeof *map);
	mape = map + n;
	for(m=map; m<map+n; m++)
		m->t = terrain;
	if(b == nil)
		return;
	for(p=b->l, m=map; m<map+n; m++, p=p->l){
		for(i=0; i<nterrain; i++){
			l = strlen(terrain[i].name);
			if(strncmp(p->name, terrain[i].name, l) == 0){
				m->t = terrain + i;
				if(strlen(p->name) > l)
					m->team = strtol(p->name+l, nil, 10);
				if(m->team < 0 || m->team > nelem(team))
					m->team = 0;
				else if(m->team > nteam)
					nteam = m->team;
				break;
			}
		}
		if((q = findblochen("unit", p, 0)) != nil)
			for(u=unit; u<unit+nunit; u++){
				l = strlen(u->name);
				if(strncmp(q->s, u->name, l) == 0){
					i = 0;
					if(strlen(q->s) > l)
						i = strtol(q->s+l, nil, 10);
					if(i < 0)
						i = 0;
					spawnunit(m, u, i);
					break;
				}
			}
	}
	popstackenblochen(b);
	for(m=map; m<map+mapwidth*mapheight; m++){
		team[m->team].income += m->t->income;
		team[m->team].nbuild++;
		if(m->t->spawn.u != nil)
			team[m->team].nprod++;
	}
}

static void
initoccupy(void)
{
	int n;
	Terrain *t;
	Unit *u;
	Stackenblochen *b, *p, *q;

	if((b = findblochen("occupy", stack, 0)) == nil)
		return;
	for(p=b->l; p!=b; p=p->l){
		for(t=terrain; t<terrain+nterrain; t++)
			if(strcmp(p->name, t->name) == 0)
				break;
		if(t == terrain + nterrain){
			fprint(2, "initoccupy: unknown terrain %s\n", p->name);
			continue;
		}
		for(q=p->r, n=0; q!=p; q=q->r)
			n++;
		t->occupy.u = emalloc(n * sizeof *t->occupy.u);
		t->occupy.e = t->occupy.u + n;
		for(q=p->r, n=0; q!=p; q=q->r){
			for(u=unit; u<unit+nunit; u++)
				if(strcmp(q->s, u->name) == 0)
					break;
			if(u == unit + nunit){
				fprint(2, "initoccupy: unknown unit %s\n", q->s);
				t->occupy.e--;
				continue;
			}
			t->occupy.u[n++] = u;
		}
	}
	popstackenblochen(b);
}

static void
initresupply(void)
{
	Terrain *t, *r;
	Stackenblochen *b, *p, *q;

	if((b = findblochen("resupply", stack, 0)) == nil)
		return;
	for(p=b->l; p!=b; p=p->l){
		for(t=terrain; t<terrain+nterrain; t++)
			if(strcmp(p->name, t->name) == 0)
				break;
		if(t == terrain + nterrain){
			fprint(2, "initresupply: unknown terrain %s\n", p->name);
			continue;
		}
		for(q=p->r; q!=p; q=q->r){
			for(r=terrain; r<terrain+nterrain; r++)
				if(strcmp(q->s, r->name) == 0)
					break;
			if(r == terrain + nterrain){
				fprint(2, "initresupply: unknown terrain %s\n", q->s);
				continue;
			}
			t->resupply = &r->spawn;
		}
	}
	popstackenblochen(b);
}

static void
initspawn(void)
{
	int n;
	Terrain *t;
	Unit *u;
	Stackenblochen *b, *p, *q;

	if((b = findblochen("spawn", stack, 0)) == nil)
		return;
	for(p=b->l; p!=b; p=p->l){
		for(t=terrain; t<terrain+nterrain; t++)
			if(strcmp(p->name, t->name) == 0)
				break;
		if(t == terrain + nterrain){
			fprint(2, "initspawn: unknown terrain %s\n", p->name);
			continue;
		}
		for(q=p->r, n=0; q!=p; q=q->r)
			n++;
		t->spawn.u = emalloc(n * sizeof *t->spawn.u);
		t->spawn.e = t->spawn.u + n;
		for(q=p->r, n=0; q!=p; q=q->r){
			for(u=unit; u<unit+nunit; u++)
				if(strcmp(q->s, u->name) == 0)
					break;
			if(u == unit + nunit){
				fprint(2, "initspawn: unknown unit %s\n", q->s);
				t->spawn.e--;
				continue;
			}
			t->spawn.u[n++] = u;
		}
	}
	popstackenblochen(b);
}

static void
initterrain(void)
{
	int n;
	Terrain *t;
	char **s;
	Stackenblochen *b, *p, *q;

	n = 1 + ((b = findblochen("terrain", stack, 0)) != nil ? b->v : 0);
	terrain = emalloc(n * sizeof *terrain);
	nterrain = n;
	t = terrain;
	t->name = "void";
	t++;
	if(n <= 1)
		return;
	for(p=b->l; t<terrain+n; t++, p=p->l){
		t->name = estrdup(p->name);
		for(q=p->r; q!=p; q=q->r){
			if(strcmp(q->name, "def") == 0){
				if((t->def = q->v) < 0)
					t->def = 0;
			}else if(strcmp(q->name, "income") == 0){
				if((t->income = q->v) < 0)
					t->income = 0;
			}else if(strcmp(q->name, "move") == 0){
				for(s=mov; s<mov+nmov; s++)
					if(strcmp(*s, q->s) == 0)
						break;
				if(s != mov + nmov)
					t->move = s - mov;
				else
					fprint(2, "initterrain: unknown move type %s\n", q->s);
			}
		}
	}
	popstackenblochen(b);
}

static void
initmove(void)
{
	int n;
	char **s;
	Unit *u;
	Stackenblochen *b, *p, *q;

	n = 1 + ((b = findblochen("move", stack, 0)) != nil ? b->v : 0);
	mov = emalloc(n * sizeof *mov);
	nmov = n;
	s = mov;
	*s++ = "void";
	for(u=unit; u<unit+nunit; u++)
		u->move = emalloc(n * sizeof *u->move);
	if(n <= 1)
		return;
	for(p=b->l, n=1; p!=b; p=p->l, n++){
		*s++ = estrdup(p->name);
		for(q=p->r; q!=p; q=q->r){
			for(u=unit; u<unit+nunit; u++)
				if(strcmp(q->name, u->name) == 0)
					break;
			if(u == unit + nunit){
				fprint(2, "initmove: unknown unit %s\n", q->name);
				continue;
			}
			u->move[n] = q->v;
		}
	}
	popstackenblochen(b);
}

static void
initunit(void)
{
	int n;
	char *s;
	Unit *u;
	Stackenblochen *b, *p, *q;
	static Unit u0 = {
		.name = "void",
		.atk = 100,
		.Δatk = 10,
		.rmin = 1,
		.rmax = 1,
		.vis = 2,
		.mp = 3,
		.cost = 1000,
	};

	n = 1 + ((b = findblochen("unit", stack, 0)) != nil ? b->v : 0);
	unit = emalloc(n * sizeof *unit);
	nunit = n;
	u = unit;
	*u++ = u0;
	if(n <= 1)
		return;
	for(p=b->l; u<unit+n; u++, p=p->l){
		*u = u0;
		u->name = estrdup(p->name);
		for(q=p->r; q!=p; q=q->r){
			if(strcmp(q->name, "atk") == 0){
				if((u->atk = q->v) < 0)
					u->atk = 0;
			}else if(strcmp(q->name, "Δatk") == 0){
				if((u->Δatk = q->v) < 0)
					u->Δatk = 0;
			}else if(strcmp(q->name, "def") == 0){
				if((u->def = q->v) < 0)
					u->def = 0;
			}else if(strcmp(q->name, "mp") == 0){
				if((u->mp = q->v) < 0)
					u->mp = 0;
			}else if(strcmp(q->name, "vis") == 0){
				if((u->vis = q->v) < 0)
					u->vis = 0;
			}else if(strcmp(q->name, "cost") == 0){
				if((u->cost = q->v) < 0)
					u->cost = 0;
			}else if(strcmp(q->name, "range") == 0){
				u->rmin = q->v;
				if((s = strrchr(q->s, ',')) != nil)
					u->rmax = strtol(s+1, nil, 10);
				if(u->rmax < u->rmin)
					u->rmax = u->rmin;
			}else if(strcmp(q->name, "unique") == 0)
				u->unique = q->v;
			else
				fprint(2, "initunit: unknown attr %s\n", q->name);
		}
	}
	popstackenblochen(b);
}

static void
initdb(void)
{
	initunit();
	initmove();
	initterrain();
	initspawn();
	initresupply();
	initoccupy();
	initmap();
	initrules();
}

void
init(void)
{
	Stackenblochen *b, *p, *q;

	loaddb(dbname, stack);
	loaddb(mapname, stack);
	initdb();
	initgame();
	for(b=stack->r; b!=stack; b=b->r){
		fprint(2, "stack %s\n", b->name);
		for(p=b->l; p!=b; p=p->l){
			fprint(2, "%s: ", p->name);
			for(q=p->r; q!=p; q=q->r)
				fprint(2, "%s=%s ", q->name, q->s);
		}
		fprint(2, "\n");
	}
}
