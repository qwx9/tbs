#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

/* FIXME:
 * - selectmove/selectspawnmove: manage corpses again
 * - db: get rid of void types
 * - fix selection/showmoverange
 * - tchs: global rule: only one move per turn, no more (and no less?)
 * - tchs: implement custom move/attack ranges and checks
 * - tchs: implement no attack (attack -> move onto unit)
 * - tchs: moving onto existing unit makes it disappear (cleanly)
 * - resupply cost (default 0)
 * - ratio, repair, destroy, launch, food, ammo, cloak lists
 * - food/fuel consumption rate list
 * - alternate attack mode ammo/...
 * - bless, curse/slow, raise lists
 * - terrain bonuses lists: atk, def, vis
 *	-> tatkm=water elemental=15
 *	-> tdefm=water elemental=3
 *	-> tvism=mountain heavy=2 (and inf=2? or any unit?)
 * - tbs2: unit and terrain sprites
 * - tbs1: sprites for rest of units
 * - manpages: tbs(1), tbs(6)?
 * - proper victory screen
 * - win condition: last team left standing (next turn -> current team)
 * - quit: confirm with changed cursor
 * - move validation and networking model
 * - ai: optional plugin for an artificial networked player
 * - savegames
 * - fow
 * - weather
 * - diplomacy/trading
 * - superteams (factions/groups), shared economies
 */

Team team[Nteam];
int nteam, curteam;
int turn, gameover;
int initmoney, unitcap, firstturnnoinc, nocorpse;
Map *saved, *unique;

typedef struct Fn Fn;
struct Fn{
	char *name;
	int (*check)(void);
	void (*fn)(void);
};

static Munit *old, *new, *savedcorpse;

static void	selectmenu(void);
static void	selecttile(void);
static void	selectaction(void);
static void	selectspawnmove(void);
static void	selectmove(void);
static void	confirmmove(void);

static void
freeunit(Map *m)
{
	free(m->u);
	m->u = nil;
}

static void
updatecorpses(void)
{
	Map *m;
	
	for(m=map; m<mape; m++)
		if(m->u != nil && m->u->decaydt && --m->u->decaydt == 0)
			freeunit(m);
}

static int
iscorpse(Map *m)
{
	Munit *mu;

	mu = m->u;
	if(mu != nil && strcmp(mu->u->name, "corpse") == 0)
		return 1;
	return 0;
}

static void
canmove_(Map *m, Map *ref, Unit *u, int mp)
{
	if(m->movep >= mp)
		return;
	if(m != ref){
		if(m->u != nil && m->u->team != curteam && !iscorpse(m))
			return;
		if(mp < u->move[m->t->move] + 1)
			return;
		mp -= u->move[m->t->move] + 1;
	}
	m->canmove = 1;
	if(mp <= 0)
		return;
	m->movep = mp;
	if((m+1-map) % mapwidth != 0)
		canmove_(m+1, ref, u, mp);
	if(m+mapwidth < mape)
		canmove_(m+mapwidth, ref, u, mp);
	if(m-mapwidth >= map)
		canmove_(m-mapwidth, ref, u, mp);
	if((m-map) % mapwidth != 0)
		canmove_(m-1, ref, u, mp);
}

static void
canmove(void)
{
	canmove_(selected2, selected2, selected2->u->u, selected2->u->u->mp);
}

static void
canattack_(Map *m, int r1, int r2)
{
	if(m->canatk)
		return;
	if(r1-- <= 0 && m != selected2){
		m->canatk = 1;
		if(m->u != nil
		&& m->u->team != curteam
		&& !iscorpse(m)
		&& selected2->u->team == curteam)
			m->atkp = 1;
	}
	if(r2-- == 0)
		return;
	if((m+1-map) % mapwidth != 0)
		canattack_(m+1, r1, r2);
	if(m+mapwidth < mape)
		canattack_(m+mapwidth, r1, r2);
	if(m-mapwidth >= map)
		canattack_(m-mapwidth, r1, r2);
	if((m-map) % mapwidth != 0)
		canattack_(m-1, r1, r2);
}

static void
canattack(void)
{
	canattack_(selected2, selected2->u->u->rmin, selected2->u->u->rmax);
}

static void
clrmoverange(int domove)
{
	Map *m;

	for(m=map; m<mape; m++){
		if(domove){
			m->movep = 0;
			m->canmove = 0;
		}
		m->atkp = 0;
		m->canatk = 0;
	}
}

static void
showmoverange(int domove)
{
	clrmoverange(domove);
	if(selected2->u == nil)
		return;
	if(domove)
		canmove();
	canattack();
}

static void
exitmenu(int domove)
{
	selected2 = saved;
	menuon = 0;
	if(domove)
		selectfn = selectspawnmove;
	else{
		saved = nil;
		selectfn = selecttile;
	}
}

static void
spawn(void)
{
	Unit *u;

	u = saved->t->spawn.u[selected2 - map];
	old = saved->u;
	spawnunit(saved, u, curteam);
	new = saved->u;
	team[curteam].money -= unique != nil && team[curteam].unique->u == u ? team[curteam].unique->ecost : u->cost;
	exitmenu(1);
}

static void
selectmenu(void)
{
	int cost;
	char *i[] = {"spawn", nil};
	void (*fn[])(void) = {spawn};
	Tunit *tu;
	Unit *u;
	Map *m;

	tu = &saved->t->spawn;
	if(tu->u + (selected2 - map) >= tu->e){
		exitmenu(0);
		return;
	}
	if(selected2 != selected)
		return;
	u = tu->u[selected2 - map];
	if(u->unique){
		if(unique != nil)
			return;
		else
			cost = team[curteam].unique->ecost;
	}else
		cost = u->cost;
	selectfn = selectmenu;
	clrmoverange(1);
	canmove_(saved, saved, u, u->mp);
	for(m=map; m<mape; m++)
		if(m->canmove)
			break;
	if(m == mape || team[curteam].money < cost)
		clrmoverange(1);
	else
		lmenu(i, fn);
}

static void
spawnlist(void)
{
	Map *m;

	unique = nil;
	if(team[curteam].unique != nil)
		for(m=map; m<mape; m++)
			if(m->u == team[curteam].unique){
				unique = m;
				break;
			}
	saved = selected2;
	selected2 = nil;
	selectfn = selectmenu;
	menuon = 1;
}

static int
checkspawn(void)
{
	if(saved != nil
	|| team[curteam].nunit >= unitcap
	|| selected2->team != curteam
	|| selected2->t->spawn.u == nil)
		return 0;
	return 1;
}

static void
endmove(void)
{
	clrmoverange(1);
	selected2->u->done = 1;
	selected2->u->decaydt = 0;
	selected2 = nil;
	if(old != nil){
		saved->u = old;
		old = nil;
	}
	new = nil;
	saved = nil;
	selectfn = selecttile;
}

static int
checkend(void)
{
	if(selected2->u == nil
	|| selected2->u->team != curteam)
		return 0;
	return 1;
}

static void
selectspawnmove(void)
{
	if(selected2 == nil || selected2 == saved && old != nil)
		goto again;
	showmoverange(0);
	redraw(0);
	if(selected2 == selected){
		selectaction();
		return;
	}else if(selected2->canmove
	&& (selected2->u == nil || iscorpse(selected2))
	&& (old == nil || selected2 != saved)){
		if(iscorpse(selected2))
			savedcorpse = selected2->u;
		selected2->u = selected->u;
		selected->u = nil;
		return;
	}
again:
	if(selected != nil && selected->u == new)
		selected->u = nil;
	saved->u = new;
	selected2 = saved;
}

static void
commitmove(void)
{
	if(saved == nil){
		selected2->u = selected->u;
		selected->u = nil;
		saved = selected;
	}else{
		saved->u = selected->u;
		selected->u = nil;
		saved = nil;
	}
}

static void
cancelmove(void)
{
	if(saved != nil){
		commitmove();
		if(savedcorpse != nil){
			selected2->u = savedcorpse;
			savedcorpse = nil;
		}
		if(old != nil){
			saved->u = old;
			old = nil;
		}
		saved = nil;
		selectfn = selecttile;
	}else
		selectfn = selectmove;
	selected2 = nil;
	clrmoverange(1);
}

static void
confirmmove(void)
{
	if(selected2 == selected){
		showmoverange(1);
		redraw(0);
		selectaction();
	}else
		cancelmove();
}

static void
selectmove(void)
{
	if(selected2 == nil)
		return;
	/* FIXME: manage corpses */
	if(selected2->canmove && selected2->u == nil){
		commitmove();
		selectfn = confirmmove;
	}else{
		selectfn = selecttile;
		selected2 = nil;
	}
}

static void
move(void)
{
	selectfn = selectmove;
}

static int
checkmove(void)
{
	if(saved != nil
	|| selected2->u == nil
	|| selected2->u->team != curteam)
		return 0;
	return 1;
}

static void
die(Map *m)
{
	Munit *mu;
	Unit *u;

	mu = m->u;
	team[mu->team].nunit--;
	if(mu == team[mu->team].unique){
		mu->ecost += mu->u->cost;
		m->u = nil;
		return;
	}
	if(!nocorpse)
		for(u=unit; u<unit+nunit; u++)
			if(strcmp(u->name, "corpse") == 0){
				mu->u = u;
				mu->decaydt = 2;
			}
}

static void
rankup(Munit *mu)
{
	mu->eatk += 2;
	mu->edef += 2;
}

static int
attackunit(Map *to, Map *from)
{
	int a, d, r, n;
	Munit *mu, *mv;

	mu = from->u;
	mv = to->u;
	r = 100;	/* FIXME: damage ratio */
	a = (mu->eatk + mu->atkm + nrand(mu->u->Δatk)) * r / 100 * mu->hp / 100;
	if(a <= 0)
		return 0;
	d = mv->edef + mu->defm + to->t->def * 5;
	n = a - d;
	if(n < 0)
		n = 0;
	if(mv->hp < n)
		n = mv->hp;
	mv->hp -= n;
	if(r = mv->hp == 0)
		die(to);
	n = mu->xp / 100;
	mu->xp += a;
	if(mu->xp / 100 > n)
		rankup(mu);
	return r;
}

static void
selectattack(void)
{
	if(!selected2->atkp)
		goto nope;
	if(attackunit(selected2, selected) == 0
	&& selected2->u->u->rmin == 1
	&& (selected2 - selected == 1
	|| selected2 - selected == -1
	|| selected2 - selected == mapwidth
	|| selected2 - selected == -mapwidth))
		attackunit(selected, selected2);
	selected2 = selected;
	endmove();
	return;
nope:
	selected2 = nil;
	selectfn = selecttile;
}

static void
attack(void)
{
	selectfn = selectattack;
}

static int
checkattack(void)
{
	Map *m;

	if(saved != nil && selected->u->u->rmin > 1
	|| selected2->u == nil
	|| selected2->u->team != curteam)
		return 0;
	for(m=map; m<mape; m++)
		if(m->atkp)
			return 1;
	return 0;
}

static void
occupy(void)
{
	team[selected2->team].nbuild--;
	team[selected2->u->team].nbuild++;
	if(selected2->t->spawn.u != nil){
		team[selected2->team].nprod--;
		team[selected2->u->team].nprod++;
	}
	selected2->team = selected2->u->team;
	team[selected2->team].income += selected2->t->income;
	endmove();
}

static int
checkoccupy(void)
{
	Unit **u;
	Tunit *tu;

	if(selected2->u == nil
	|| selected2->team == curteam)
		return 0;
	tu = &selected2->t->occupy;
	if(selected2->u->team == curteam){
		for(u=tu->u; u<tu->e; u++)
			if(*u == selected2->u->u)
				return 1;
	}
	return 0;
}

static Fn fn[] = {
	{"spawn", checkspawn, spawnlist},
	{"occupy", checkoccupy, occupy},
	{"attack", checkattack, attack},
	{"move", checkmove, move},
	{"end", checkend, endmove},
};

static void
selectaction(void)
{
	char *i[nelem(fn)+1] = {nil}, **ip = i;
	void (*fns[nelem(fn)])(void) = {nil}, (**fp)(void) = fns;
	Fn *f;

	if(selected2 != selected)
		goto end;
	for(f=fn; f<fn+nelem(fn); f++)
		if(f->check()){
			*ip++ = f->name;
			*fp++ = f->fn;
		}
	if(ip > i)
		lmenu(i, fns);
	else{
end:
		clrmoverange(1);
		selectfn = selecttile;
	}
}

static void
selecttile(void)
{
	selectfn = selectaction;
	showmoverange(1);
}

void
spawnunit(Map *m, Unit *u, int t)
{
	Munit *mu;

	if(t < 0 || t > nelem(team))
		t = 0;
	if(u->unique && curteam != 0 && team[curteam].unique != nil){
		mu = team[curteam].unique;
	}else{
		mu = emalloc(sizeof *m->u);
		if(t > nteam)
			nteam = t;
		if(u->unique){
			if(team[t].unique != nil)
				sysfatal("spawnunit: team %d uniqueness violation\n", t);
			team[t].unique = mu;
		}
		mu->u = u;
		mu->team = t;
		mu->ecost = u->cost;
		mu->eatk = u->atk;
		mu->edef = u->def;
	}
	mu->hp = 100;
	team[t].nunit++;
	m->u = mu;
}

static void
resupply(void)
{
	Map *m;
	Unit **u;
	Munit *mu;
	Terrain *t;
	Tunit *tu;

	for(m=map; m<mape; m++){
		mu = m->u;
		t = m->t;
		tu = t->resupply;
		if(mu == nil
		|| mu->team != curteam
		|| mu->hp == 100
		|| tu == nil)
			continue;
		for(u=tu->u; u<tu->e; u++)
			if(mu->u == *u)
				break;
		if(u == tu->e)
			continue;
		mu->hp += 20;
		if(mu->hp > 100)
			mu->hp = 100;
	}
}

static void
newturn(void)
{
	if(turn > 0 || !firstturnnoinc)
		team[curteam].money += team[curteam].income;
	resupply();
}

void
endturn(void)
{
	Team *t;
	Map *m;

	if(saved != nil)
		cancelmove();
	menuon = 0;
	selectfn = selecttile;
	selected = selected2 = nil;
	for(m=map; m<mape; m++)
		if(m->u != nil)
			m->u->done = 0;
	for(t=team+curteam+1; t<=team+nteam; t++)
		if(t->nunit > 0 || t->nprod > 0)
			break;
	if(t == team+nteam+1){
		turn++;
		updatecorpses();
		for(t=team+1; t<team+curteam; t++)
			if(t->nunit > 0 || t->nprod > 0)
				break;
	}
	if(t == team+curteam)
		gameover = 1;
	curteam = t - team;
	newturn();
	redraw(0);
}

void
initgame(void)
{
	Team *t;

	for(t=team; t<=team+nteam; t++){
		t->money = initmoney;
		if(t > team && curteam == 0
		&& (t->nunit > 0 || t->nprod > 0))
			curteam = t - team;
	}
	if(curteam == 0)
		sysfatal("initgame: the only winning move is not to play");
	newturn();
	selectfn = selecttile;
}
