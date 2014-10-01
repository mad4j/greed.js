/* 
 * greed.c - Written by Matthew T. Day (mday@iconsys.uu.net), 09/06/89
 *
 * Now maintained by Eric S. Raymond <esr@snark.thyrsus.com>.  Matt
 * Day dropped out of sight and hasn't posted a new version or patch
 * in many years.
 *
 * 11/15/95 Fred C. Smith (Yes, the SAME Fred C. Smith who did the MS-DOS
 *          version), fix the 'p' option so when it removes the highlight
 *          from unused possible moves, it restores the previous color.
 *          -Some minor changes in the way it behaves at the end of a game,
 *          because I didn't like the way someone had changed it to work
 *          since I saw it a few years ago (personal preference).
 *          -Some style changes in the code, again personal preference.
 *          fredex@fcshome.stoneham.ma.us
 */

/*
 * When using a curses library with color capability, Greed will
 * detect color curses(3) if you have it and generate the board in
 * color, one color to each of the digit values. This will also enable
 * checking of an environment variable GREEDOPTS to override the
 * default color set, which will be parsed as a string of the form:
 *
 *	<c1><c2><c3><c4><c5><c6><c7><c8><c9>[:[p]]
 *
 * where <cn> is a character decribing the color for digit n.
 * The color letters are read as follows:
 *	b = blue,
 *	g = green,
 *	c = cyan,
 *	r = red,
 *	m = magenta,
 *	y = yellow,
 *	w = white.
 * In addition, capitalizing a letter turns on the A_BOLD attribute for that
 * letter.
 *
 * If the string ends with a trailing :, letters following are taken as game
 * options. At present, only 'p' (equivalent to an initial 'p' command) is
 * defined.
 */

static char *version = "Greed v" RELEASE;

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <pwd.h>
#include <fcntl.h>
#include <stdbool.h>
#ifdef A_COLOR
#include <ctype.h>
#endif

#define HEIGHT	22
#define WIDTH	79
#define ME	'@'

/*
 * The scorefile is fixed-length binary and consists of
 * structure images - very un-Unixy design!
 */
#define MAXSCORES 10
#define SCOREFILESIZE (MAXSCORES * sizeof(struct score))

/* rnd() returns a random number between 1 and x */
#define rnd(x) (int) ((lrand48() % (x))+1)

#define LOCKPATH "/tmp/Greed.lock"	/* lock path for high score file */

/* 
 * changing stuff in this struct
 * makes old score files incompatible
 */
struct score {
    char user[9];
    int score;
};

static int grid[HEIGHT][WIDTH], y, x;
static bool allmoves = false, havebotmsg = false;
static int score = 0;
static char *cmdname;
static WINDOW *helpwin = NULL;

static void topscores(int);

static void botmsg(char *msg, bool backcur)
/* 
 * botmsg() writes "msg" at the middle of the bottom line of the screen.
 * Boolean "backcur" specifies whether to put cursor back on the grid or
 * leave it on the bottom line (e.g. for questions).
 */
{
    mvaddstr(23, 40, msg);
    clrtoeol();
    if (backcur) 
	move(y, x);
    refresh();
    havebotmsg = true;
}


static void quit(int sig) 
/* 
 * quit() is run when the user hits ^C or ^\, it queries the user if he
 * really wanted to quit, and if so, checks the high score stuff (with the
 * current score) and quits; otherwise, simply returns to the game.
 */
{
    int ch;
    void (*osig)() = signal(SIGINT, SIG_IGN);	/* save old signal */
    (void) signal(SIGQUIT, SIG_IGN);

    if (stdscr) {
	botmsg("Really quit? ", false);
	if ((ch = getch()) != 'y' && ch != 'Y') {
	    move(y, x);
	    (void) signal(SIGINT, osig);	/* reset old signal */
	    (void) signal(SIGQUIT, osig);
	    refresh();
	    return;
	}
	move(23, 0);
	refresh();
	endwin();
	puts("\n");
	topscores(score);
    }
    exit(0);
}

static void out(int onsig)
/* 
 * out() is run when the signal SIGTERM is sent, it corrects the terminal
 * state (if necessary) and exits.
 */
{
    if (stdscr) endwin();
    exit(0);
}


static void usage(void) 
/* usage() prints out the proper command line usage for Greed and exits. */
{
    fprintf(stderr, "Usage: %s [-p] [-s]\n", cmdname);
    exit(1);
}

static void showscore(void) 
/* 
 * showscore() prints the score and the percentage of the screen eaten
 * at the beginning of the bottom line of the screen, moves the
 * cursor back on the grid, and refreshes the screen.
 */
{
    mvprintw(23, 7, "%d  %.2f%%", score, (float) score / 17.38);
    move(y, x);
    refresh();
}

void showmoves(bool, int*);

main(int argc, char **argv)
{
    int val = 1;
    extern long time();
    int attribs[9];
#ifdef A_COLOR
    char *colors;
#endif

    cmdname = argv[0];			/* save the command name */
    if (argc == 2) {			/* process the command line */
	if (strlen(argv[1]) != 2 || argv[1][0] != '-') usage();
	if (argv[1][1] == 's') {
	    topscores(0);
	    exit(0);
	}
    } 
    else if (argc > 2)		/* can't have > 2 arguments */ 
	usage();

    (void) signal(SIGINT, quit);	/* catch off the signals */
    (void) signal(SIGQUIT, quit);
    (void) signal(SIGTERM, out);

    initscr();				/* set up the terminal modes */
#ifdef KEY_MIN
    keypad(stdscr, true);
#endif /* KEY_MIN */
    cbreak();
    noecho();

    srand48(time(0) ^ getpid() << 16);	/* initialize the random seed *
					 * with a unique number       */

#ifdef A_COLOR
    if (has_colors()) {
	start_color();
	init_pair(1, COLOR_YELLOW, COLOR_BLACK);
	init_pair(2, COLOR_RED, COLOR_BLACK);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);	
	init_pair(5, COLOR_MAGENTA, COLOR_BLACK);

	attribs[0] = COLOR_PAIR(1);
	attribs[1] = COLOR_PAIR(2);
	attribs[2] = COLOR_PAIR(3);
	attribs[3] = COLOR_PAIR(4);
	attribs[4] = COLOR_PAIR(5);
	attribs[5] = COLOR_PAIR(1) | A_BOLD;
	attribs[6] = COLOR_PAIR(2) | A_BOLD;
	attribs[7] = COLOR_PAIR(3) | A_BOLD;
	attribs[8] = COLOR_PAIR(4) | A_BOLD;

	if ((colors = getenv("GREEDOPTS")) != (char *) NULL) {
	    static char *cnames = " rgybmcwRGYBMCW";
	    char *cp;

	    for (cp = colors; *cp && *cp != ':'; cp++)
		if (strchr(cnames, *cp) != (char *) NULL)
		    if (*cp != ' ') {
			init_pair(cp-colors+1,
				  strchr(cnames, tolower(*cp))-cnames,
				  COLOR_BLACK);
			attribs[cp-colors]=COLOR_PAIR(cp-colors+1);
			if (isupper(*cp))
			    attribs[cp-colors] |= A_BOLD;
		    }
	    if (*cp == ':')
		while (*++cp)
		    if (*cp == 'p')
			allmoves = true;
	}
    }
#endif

    for (y=0; y < HEIGHT; y++)		/* fill the grid array and */
	for (x=0; x < WIDTH; x++)		/* print numbers out */
#ifdef A_COLOR
	    if (has_colors()) {
		int newval = rnd(9);

		attron(attribs[newval - 1]);
		mvaddch(y, x, (grid[y][x] = newval) + '0');
		attroff(attribs[newval - 1]);
	    } else
#endif
		mvaddch(y, x, (grid[y][x] = rnd(9)) + '0');

    mvaddstr(23, 0, "Score: ");		/* initialize bottom line */
    mvprintw(23, 40, "%s - Hit '?' for help.", version);
    y = rnd(HEIGHT)-1; x = rnd(WIDTH)-1;		/* random initial location */
    standout();
    mvaddch(y, x, ME);
    standend();
    grid[y][x] = 0;				/* eat initial square */

    if (allmoves) 
	showmoves(true, attribs);
    showscore();

    /* main loop, gives tunnel() a user command */
    while ((val = tunnel(getch(), attribs)) > 0)
	continue;

    if (!val) {				/* if didn't quit by 'q' cmd */
	botmsg("Hit any key..", false);	/* then let user examine     */
	getch();			/* final screen              */
    }

    move(23, 0);
    refresh();
    endwin();
    puts("\n");				/* writes two newlines */
    topscores(score);
    exit(0);
}


int tunnel(chtype cmd, int *attribs)
/* 
 * tunnel() does the main game work.  Returns 1 if everything's okay, 0 if
 * user "died", and -1 if user specified and confirmed 'q' (fast quit).
 */
{
    int dy, dx, distance;
    void help(void);

    switch (cmd) {				/* process user command */
    case 'h': case 'H': case '4':
#ifdef KEY_LEFT
    case KEY_LEFT:
#endif /* KEY_LEFT */
	dy = 0; dx = -1;
	break;
    case 'j': case 'J': case '2':
#ifdef KEY_DOWN
    case KEY_DOWN:
#endif /* KEY_DOWN */
	dy = 1; dx = 0;
	break;
    case 'k': case 'K': case '8':
#ifdef KEY_UP
    case KEY_UP:
#endif /* KEY_UP */
	dy = -1; dx = 0;
	break;
    case 'l': case 'L': case '6':
#ifdef KEY_RIGHT
    case KEY_RIGHT:
#endif /* KEY_RIGHT */
	dy = 0; dx = 1;
	break;
    case 'b': case 'B': case '1':
	dy = 1; dx = -1;
	break;
    case 'n': case 'N': case '3':
	dy = dx = 1;
	break;
    case 'y': case 'Y': case '7':
	dy = dx = -1;
	break;
    case 'u': case 'U': case '9':
	dy = -1; dx = 1;
	break;
    case 'p': case 'P':
	allmoves = !allmoves;
	showmoves(allmoves, attribs);
	move(y, x);
	refresh();
	return (1);
    case 'q': case 'Q':
	quit(0);
	return(1);
    case '?':
	help();
	return (1);
    case '\14': case '\22':			/* ^L or ^R (redraw) */
	wrefresh(curscr);		/* falls through to return */
    default:
	return (1);
    }
    distance = (y+dy >= 0 && x+dx >= 0 && y+dy < HEIGHT && x+dx < WIDTH) ?
	grid[y+dy][x+dx] : 0;

    {
	int j = y, i = x, d = distance;

	do {				/* process move for validity */
	    j += dy;
	    i += dx;
	    if (j >= 0 && i >= 0 && j < HEIGHT && i < WIDTH && grid[j][i])
		continue;	/* if off the screen */
	    else if (!othermove(dy, dx)) {	/* no other good move */
		j -= dy;
		i -= dx;
		mvaddch(y, x, ' ');
		while (y != j || x != i) {
		    y += dy;	/* so we manually */
		    x += dx;	/* print chosen path */
		    score++;
		    mvaddch(y, x, ' ');
		}
		mvaddch(y, x, '*');	/* with a '*' */
		showscore();		/* print final score */
		return (0);
	    } else {		/* otherwise prevent bad move */
		botmsg("Bad move.", true);
		return (1);
	    }
	} while (--d);
    }

    /* remove possible moves */
    if (allmoves) 
	showmoves(false, attribs);

    if (havebotmsg) {			/* if old bottom msg exists */
	mvprintw(23, 40, "%s - Hit '?' for help.", version);
	havebotmsg = false;
    }

    mvaddch(y, x, ' ');			/* erase old ME */
    do {				/* print good path */
	y += dy;
	x += dx;
	score++;
	grid[y][x] = 0;
	mvaddch(y, x, ' ');
    } while (--distance);
    standout();
    mvaddch(y, x, ME);			/* put new ME */
    standend();
    if (allmoves) 
	showmoves(true, attribs);		/* put new possible moves */
    showscore();			/* does refresh() finally */
    return (1);
}

int othermove(int bady, int badx)
/* 
 * othermove() checks area for an existing possible move.  bady and
 * badx are direction variables that tell othermove() they are
 * already no good, and to not process them.  I don't know if this
 * is efficient, but it works!
 */
{
    int dy = -1, dx;

    for (; dy <= 1; dy++)
	for (dx = -1; dx <= 1; dx++)
	    if ((!dy && !dx) || (dy == bady && dx == badx) ||
		y+dy < 0 && x+dx < 0 && y+dy >= HEIGHT && x+dx >= WIDTH)
		/* don't do 0,0 or bad coordinates */
		continue;
	    else {
		int j=y, i=x, d=grid[y+dy][x+dx];

		if (!d) continue;
		do {		/* "walk" the path, checking */
		    j += dy;
		    i += dx;
		    if (j < 0 || i < 0 || j >= HEIGHT ||
			i >= WIDTH || !grid[j][i]) break;
		} while (--d);
		if (!d) return 1;	/* if "d" got to 0, *
					 * move was okay.   */
	    }
    return 0;			/* no good moves were found */
}

void showmoves(bool on, int *attribs)
/*
 * showmoves() is nearly identical to othermove(), but it highlights possible
 * moves instead.  "on" tells showmoves() whether to add or remove moves.
 */
{
    int dy = -1, dx;

    for (; dy <= 1; dy++) {
	if (y+dy < 0 || y+dy >= HEIGHT) continue;
	for (dx = -1; dx <= 1; dx++) {
	    int j=y, i=x, d=grid[y+dy][x+dx];

	    if (!d) continue;
	    do {
		j += dy;
		i += dx;
		if (j < 0 || i < 0 || j >= HEIGHT
		    || i >= WIDTH || !grid[j][i]) break;
	    } while (--d);
	    if (!d) {
		int j=y, i=x, d=grid[y+dy][x+dx];

		/* The next section chooses inverse-video    *
		 * or not, and then "walks" chosen valid     *
		 * move, reprinting characters with new mode */

		if (on)
		    standout();
		do {
		    j += dy;
		    i += dx;
#ifdef A_COLOR
		    if (!on && has_colors()) {
			int newval = grid[j][i];
			attron(attribs[newval - 1]);
			mvaddch(j, i, newval + '0');
			attroff(attribs[newval - 1]);
		    }
		    else
#endif
			mvaddch(j, i, grid[j][i] + '0');
		} while (--d);
		if (on) standend();
	    }
	}
    }
}

char doputc(char c)
/* doputc() simply prints out a character to stdout, used by tputs() */
{
    return(fputc(c, stdout));
}

static void topscores(int newscore)
/* 
 * topscores() processes its argument with the high score file, makes any
 * updates to the file, and outputs the list to the screen.  If "newscore"
 * is false, the score file is printed to the screen (i.e. "greed -s")
 */
{
    int fd, count = 1;
    static char termbuf[BUFSIZ];
    char *tptr = (char *) malloc(16), *boldon, *boldoff;
    struct score *toplist = (struct score *) malloc(SCOREFILESIZE);
    struct score *ptrtmp, *eof = &toplist[MAXSCORES], *new = NULL;
    extern char *getenv(), *tgetstr();
    void lockit(bool);

    (void) signal(SIGINT, SIG_IGN);	/* Catch all signals, so high */
    (void) signal(SIGQUIT, SIG_IGN);	/* score file doesn't get     */
    (void) signal(SIGTERM, SIG_IGN);	/* messed up with a kill.     */
    (void) signal(SIGHUP, SIG_IGN);

    /* following open() creates the file if it doesn't exist
     * already, using secure mode
     */
    if ((fd = open(SCOREFILE, O_RDWR|O_CREAT, 0600)) == -1) {
	    fprintf(stderr, "%s: %s: Cannot open.\n", cmdname,
		    SCOREFILE);
	exit(1);
    }

    lockit(true);			/* lock score file */
    for (ptrtmp=toplist; ptrtmp < eof; ptrtmp++) ptrtmp->score = 0;
    /* initialize scores to 0 */
    read(fd, toplist, SCOREFILESIZE);	/* read whole score file in at once */

    if (newscore) {			/* if possible high score */
	for (ptrtmp=toplist; ptrtmp < eof; ptrtmp++)
	    /* find new location for score */
	    if (newscore > ptrtmp->score) break;
	if (ptrtmp < eof) {	/* if it's a new high score */
	    new = ptrtmp;	/* put "new" at new location */
	    ptrtmp = eof-1;	/* start at end of list */
	    while (ptrtmp > new) {	/* shift list one down */
		*ptrtmp = *(ptrtmp-1);
		ptrtmp--;
	    }

	    new->score = newscore;	/* fill "new" with the info */
	    strncpy(new->user, getpwuid(getuid())->pw_name, 8);
	    (void) lseek(fd, 0, 0);	/* seek back to top of file */
	    write(fd, toplist, SCOREFILESIZE);	/* write it all out */
	}
    }

    close(fd);
    lockit(false);			/* unlock score file */

    if (toplist->score) 
	puts("Rank  Score  Name     Percentage");
    else 
	puts("No high scores.");	/* perhaps "greed -s" was run before *
					 * any greed had been played? */
    if (new && tgetent(termbuf, getenv("TERM")) > 0) {
	/* grab escape sequences for standout */
	boldon = tgetstr("so", &tptr);
	boldoff = tgetstr("se", &tptr);
	/* if only got one of the codes, use neither */
	if (boldon==NULL || boldoff==NULL) 
	    boldon = boldoff = NULL;
    }

    /* print out list to screen, highlighting new score, if any */
    for (ptrtmp=toplist; ptrtmp < eof && ptrtmp->score; ptrtmp++, count++) {
	if (ptrtmp == new && boldon)
	    tputs(boldon, 1, doputc);
	printf("%-5d %-6d %-8s %.2f%%\n", count, ptrtmp->score,
	       ptrtmp->user, (float) ptrtmp->score / 17.38);
	if (ptrtmp == new && boldoff) tputs(boldoff, 1, doputc);
    }
}


void lockit(bool on)
/*
 * lockit() creates a file with mode 0 to serve as a lock file.  The creat()
 * call will fail if the file exists already, since it was made with mode 0.
 * lockit() will wait approx. 15 seconds for the lock file, and then
 * override it (shouldn't happen, but might).  "on" says whether to turn
 * locking on or not.
 */
{
    int fd, x = 1;

    if (on) {
	while ((fd = open(LOCKPATH, O_RDWR | O_CREAT | O_EXCL, 0)) < 0) {
	    printf("Waiting for scorefile access... %d/15\n", x);
	    if (x++ >= 15) {
		puts("Overriding stale lock...");
		if (unlink(LOCKPATH) == -1) {
		    fprintf(stderr,
			    "%s: %s: Can't unlink lock.\n",
			    cmdname, LOCKPATH);
		    exit(1);
		}
	    }
	    sleep(1);
	}
	close(fd);
    } else unlink(LOCKPATH);
}

#define msg(row, msg) mvwaddstr(helpwin, row, 2, msg);

void help(void) 
/* 
 * help() simply creates a new window over stdscr, and writes the help info
 * inside it.  Uses macro msg() to save space.
 */
{
    if (!helpwin) {
	helpwin = newwin(18, 65, 1, 7);
#ifdef ACS_URCORNER
	box(helpwin, ACS_VLINE, ACS_HLINE);	/* print box around info */
	(void) waddch(helpwin, ACS_ULCORNER); 
	mvwaddch(helpwin, 0, 64, ACS_URCORNER);
	mvwaddch(helpwin, 17, 0, ACS_LLCORNER); 
	mvwaddch(helpwin, 17, 64, ACS_LRCORNER);
#else
	box(helpwin, '|', '-');	/* print box around info */
	/* put '+' at corners, looks better */
	(void) waddch(helpwin, '+'); mvwaddch(helpwin, 0, 64, '+');
	mvwaddch(helpwin, 17, 0, '+'); mvwaddch(helpwin, 17, 64, '+');
#endif

	mvwprintw(helpwin, 1, 2,
		  "Welcome to %s, by Matthew Day <mday@iconsys.uu.net>.",version);
	msg(3, " The object of Greed is to erase as much of the screen as");
	msg(4, " possible by moving around in a grid of numbers.  To move,");
	msg(5, " use the arrow keys, your number pad, or one of the letters");
	mvwprintw(helpwin, 6, 2,
	       " 'hjklyubn'. Your location is signified by the '%c' symbol.", ME);
	msg(7, " When you move in a direction, you erase N number of grid");
	msg(8, " squares in that direction, N being the first number in that");
	msg(9, " direction.  Your score reflects the total number of squares");
	msg(10," eaten.  Greed will not let you make a move that would have");
	msg(11," placed you off the grid or over a previously eaten square");
	msg(12," unless no valid moves exist, in which case your game ends.");
	msg(13," Other Greed commands are 'Ctrl-L' to redraw the screen,");
	msg(14," 'p' to toggle the highlighting of the possible moves, and");
	msg(15," 'q' to quit.  Command line options to Greed are '-s' to");
	msg(16," output the high score file.");

	(void) wmove(helpwin, 17, 64);
	wrefresh(helpwin);
    } else {
	touchwin(helpwin);
	wrefresh(helpwin);
    }
    (void) wgetch(helpwin);
    touchwin(stdscr);
    refresh();
}

/* the end */
