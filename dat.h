typedef struct Unit Unit;
typedef struct Munit Munit;
typedef struct Tunit Tunit;
typedef struct Terrain Terrain;
typedef struct Map Map;
typedef struct Team Team;

extern char **mov;
extern int nmov;

struct Unit{
	char *name;
	u32int **pic;
	int atk;
	int Δatk;
	int rmin;
	int rmax;
	int def;
	int *move;
	int mp;
	int vis;
	int cost;
	int unique;
};
extern Unit *unit;
extern int nunit;

struct Munit{
	Unit *u;
	int team;
	int atkm;
	int defm;
	int eatk;
	int edef;
	int ecost;
	int xp;
	int hp;
	int done;
	int decaydt;
};

struct Tunit{
	Unit **u;
	Unit **e;
};

struct Terrain{
	char *name;
	u32int **pic;
	int move;
	int def;
	int income;
	Tunit spawn;
	Tunit occupy;
	Tunit *resupply;
};
extern Terrain *terrain;
extern int nterrain;

struct Map{
	Terrain *t;
	int team;
	Munit *u;
	int movep;
	int canmove;
	int atkp;
	int canatk;
};
extern Map *map, *mape, *selected, *selected2, *saved, *unique;
extern int mapwidth, mapheight;

enum{
	Nteam = 64,
};

struct Team{
	int money;
	int income;
	int nunit;
	int nbuild;
	int nprod;
	Munit *unique;
};
extern Team team[Nteam];
extern int nteam, curteam;
extern int turn, gameover;
extern int initmoney, unitcap, firstturnnoinc, nocorpse;

enum{
	Pcur,
	P1,
	P2,
	P3,
	P4,
	P5,
	P6,
	P7,
	P8,
	P9,
	Pend
};

extern void (*selectfn)(void);
extern int scale;
extern int menuon;

extern char *progname, *prefix, *dbname, *mapname;
