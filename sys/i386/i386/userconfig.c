#include <sys/param.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/isa.c>
#include <machine/console.h>
#include <machine/limits.h>

#define PARM_DEVSPEC	0x1
#define PARM_INT	0x2
#define PARM_ADDR	0x3

typedef struct _cmdparm {
    int type;
    union {
	struct isa_device *dparm;
	int iparm;
	void *aparm;
    } parm;
} CmdParm;

typedef int (*CmdFunc)(CmdParm *);

typedef struct _cmd {
    char *name;
    CmdFunc handler;
    CmdParm *parms;
} Cmd;

static void lsdevtab(struct isa_device *);
static struct isa_device *find_device(char *, int);
static struct isa_device *search_devtable(struct isa_device *, char *, int);
static void cngets(char *, int);
static Cmd *parse_cmd(char *);
static int parse_args(char *, CmdParm *);
long strtol(const char *, char **, int);
int strncmp(const char *, const char *, size_t);

static int list_devices(CmdParm *);
static int set_device_ioaddr(CmdParm *);
static int set_device_irq(CmdParm *);
static int set_device_drq(CmdParm *);
static int set_device_mem(CmdParm *);
static int set_device_enable(CmdParm *);
static int set_device_disable(CmdParm *);
static int quitfunc(CmdParm *);
static int helpfunc(CmdParm *);

CmdParm addr_parms[] = {
    { PARM_DEVSPEC, {} },
    { PARM_ADDR, {} },
    { -1, {} },
};

CmdParm int_parms[] = {
    { PARM_DEVSPEC, {} },
    { PARM_INT, {} },
    { -1, {} },
};

CmdParm dev_parms[] = {
    { PARM_DEVSPEC, {} },
    { -1, {} },
};

Cmd CmdList[] = {
    { "ls",	list_devices,		NULL },		/* ls		*/
    { "list",	list_devices,		NULL },		/* ""		*/
    { "io",	set_device_ioaddr,	addr_parms },	/* io dev addr	*/
    { "irq",	set_device_irq,		int_parms },	/* irq dev #	*/
    { "drq",	set_device_drq,		int_parms },	/* drq dev #	*/
    { "mem",	set_device_mem,		int_parms },	/* mem dev #	*/
    { "enable",	set_device_enable,	dev_parms },	/* enable dev	*/
    { "disable", set_device_disable,	dev_parms },	/* disable dev	*/
    { "quit", 	quitfunc, 		NULL },		/* quit		*/
    { "q", 	quitfunc, 		NULL },		/* ""		*/
    { "exit", 	quitfunc, 		NULL },		/* ""		*/
    { "help", 	helpfunc, 		NULL },		/* help		*/
    { "?", 	helpfunc, 		NULL },		/* ""		*/
    { NULL,	NULL,			NULL },
};

void
userconfig(void)
{
    char command[80];
    char input[80];
    int rval;
    struct isa_device *dt;
    Cmd *cmd;

    while (1) {
	printf("config> ");
	cngets(input, 80);
	if (input[0] == NULL)
	    continue;
	cmd = parse_cmd(input);
	if (!cmd) {
	    printf("Invalid command or syntax.  Type `?' for help.\n");
	    continue;
	}
	rval = (*cmd->handler)(cmd->parms);
	if (rval)
	    return;
    }
}

static Cmd *
parse_cmd(char *cmd)
{
    Cmd *cp;

    for (cp = CmdList; cp->name; cp++) {
	int len = strlen(cp->name);

	if (!strncmp(cp->name, cmd, len)) {
	    if (parse_args(cmd + len, cp->parms))
		return NULL;
	    else
		return cp;
	}
    }
    return NULL;
}

static int
parse_args(char *cmd, CmdParm *parms)
{
    while (*cmd) {
	char *ptr;

	if (parms->type == -1)
	    return 0;
	if (*cmd == ' ' || *cmd == '\t') {
	    ++cmd;
	    continue;
	}
	if (parms->type == PARM_DEVSPEC) {
	    int i = 0;
	    char devname[64];
	    int unit = 0;

	    while (*cmd && !(*cmd == ' ' || *cmd == '\t' ||
	      (*cmd >= '0' && *cmd <= '9')))
		devname[i++] = *(cmd++);
	    devname[i] = NULL;
	    if (*cmd >= '0' && *cmd <= '9') {
		unit = strtol(cmd, &ptr, 10);
		cmd = ptr;
	    }
	    if ((parms->parm.dparm = find_device(devname, unit)) == NULL) {
	        printf("No such device: %s%d\n", devname, unit);
		return 1;
	    }
	    ++parms;
	    continue;
	}
	if (parms->type == PARM_INT) {
	    parms->parm.iparm = strtol(cmd, &ptr, 10);
	    if (cmd == ptr) {
	        printf("Invalid numeric argument\n");
		return 1;
	    }
	    cmd = ptr;
	    ++parms;
	    continue;
	}
	if (parms->type == PARM_ADDR) {
	    parms->parm.aparm = (void *)strtol(cmd, &ptr, 16);
	    if (cmd == ptr) {
	        printf("Invalid address argument\n");
	        return 1;
	    }
	    cmd = ptr;
	    ++parms;
	    continue;
	}
    }
    return 0;
}

static int
list_devices(CmdParm *parms)
{
    printf("Device	IOaddr	IRQ	DRQ	MemAddr	Flags	Enabled\n");
    lsdevtab(&isa_devtab_bio[0]);
    lsdevtab(&isa_devtab_tty[0]);
    lsdevtab(&isa_devtab_net[0]);
    lsdevtab(&isa_devtab_null[0]);
    return 0;
}

static int
set_device_ioaddr(CmdParm *parms)
{
    parms[0].parm.dparm->id_iobase = parms[1].parm.aparm;
    return 0;
}

static int
set_device_irq(CmdParm *parms)
{
    parms[0].parm.dparm->id_irq = parms[1].parm.iparm;
    return 0;
}

static int
set_device_drq(CmdParm *parms)
{
    parms[0].parm.dparm->id_drq = parms[1].parm.iparm;
    return 0;
}

static int
set_device_mem(CmdParm *parms)
{
    parms[0].parm.dparm->id_maddr = parms[1].parm.aparm;
    return 0;
}

static int
set_device_enable(CmdParm *parms)
{
    return 0;
}

static int
set_device_disable(CmdParm *parms)
{
    return 0;
}

static int
quitfunc(CmdParm *parms)
{
    return 1;
}

static int
helpfunc(CmdParm *parms)
{
    printf("Command\t\tDescription\n");
    printf("-------\t\t-----------\n");
    printf("ls\t\t\tList currently configured devices\n");
    printf("io <devname> <addr>\tSet device io address\n");
    printf("irq <devname> <irq>\tSet device IRQ\n");
    printf("drq <devname> <drq>\tSet device DRQ\n");
    printf("mem <devname> <addr>\tSet device memory address\n");
    printf("enable <devname>\tEnable device\n");
    printf("disable <devname>\tDisable device (will not be probed)\n");
    printf("quit\t\t\tExit this configuration utility\n");
    printf("help\t\t\tThis message\n");
    return 0;
}

static void
lsdevtab(struct isa_device *dt)
{
    int i;

    for (i = 0; dt->id_id != 0; dt++) {
	printf("%s%d	0x%x	%d	%d	0x%x	0x%x	%s\n",
 	       dt->id_driver->name, dt->id_unit, dt->id_iobase,
	       ffs(dt->id_irq) - 1, dt->id_drq, dt->id_maddr,
	       dt->id_flags, "Yes");
    }
}

static struct isa_device *
find_device(char *devname, int unit)
{
    struct isa_device *ret;

    if ((ret = search_devtable(&isa_devtab_bio[0], devname, unit)) != NULL)
        return ret;
    if ((ret = search_devtable(&isa_devtab_tty[0], devname, unit)) != NULL)
        return ret;
    if ((ret = search_devtable(&isa_devtab_net[0], devname, unit)) != NULL)
        return ret;
    if ((ret = search_devtable(&isa_devtab_null[0], devname, unit)) != NULL)
        return ret;
    return NULL;
}

static struct isa_device *
search_devtable(struct isa_device *dt, char *devname, int unit)
{
    int i;

    for (i = 0; dt->id_id != 0; dt++)
        if (!strcmp(dt->id_driver->name, devname) && dt->id_unit == unit)
	    return dt;
    return NULL;
}

void
cngets(char *input, int maxin)
{
    int c, nchars = 0;

    while (1) {
	c = cngetc();
	if (c == NOKEY)
	    continue;
	/* Treat ^H or ^? as backspace */
	if ((c == '\010' || c == '\177')) {
	    	if (nchars) {
			printf("\010 \010");
			*--input = NULL, --nchars;
		}
		continue;
	}
	/* Treat ^U or ^X as kill line */
	else if ((c == '\025' || c == '\030')) {
		while (nchars--) {
			*--input = NULL;
			printf("\010 \010");
		}
		continue;
	}
	printf("%c", c);
	if ((++nchars == maxin) || (c == '\n')) {
	    *input = NULL;
	    break;
	}
	*input++ = (u_char)c;
    }
}

int
strncmp(const char *s1, const char *s2, size_t n)
{

    if (n == 0)
	return (0);
    do {
	if (*s1 != *s2++)
	    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
	if (*s1++ == 0)
	    break;
    } while (--n != 0);
    return (0);
}

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 *
 * Slightly lobotomized for inclusion here.
 */
long
strtol(const char *nptr, char **endptr, int base)
{
    register const char *s = nptr;
    register unsigned long acc;
    register int c;
    register unsigned long cutoff;
    register int neg = 0, any, cutlim;

    /*
     * Skip white space and pick up leading +/- sign if any.
     * If base is 0, allow 0x for hex and 0 for octal, else
     * assume decimal; if base is already 16, allow 0x.
     */
    do {
	c = *s++;
    } while (c == ' ' || c == '\t');
    if (c == '-') {
	neg = 1;
	c = *s++;
    } else if (c == '+')
	c = *s++;
    if ((base == 0 || base == 16) &&
	c == '0' && (*s == 'x' || *s == 'X')) {
	c = s[1];
	s += 2;
	base = 16;
    }
    if (base == 0)
	base = c == '0' ? 8 : 10;

    /*
     * Compute the cutoff value between legal numbers and illegal
     * numbers.  That is the largest legal value, divided by the
     * base.  An input number that is greater than this value, if
     * followed by a legal input character, is too big.  One that
     * is equal to this value may be valid or not; the limit
     * between valid and invalid numbers is then based on the last
     * digit.  For instance, if the range for longs is
     * [-2147483648..2147483647] and the input base is 10,
     * cutoff will be set to 214748364 and cutlim to either
     * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
     * a value > 214748364, or equal but the next digit is > 7 (or 8),
     * the number is too big, and we will return a range error.
     *
     * Set any if any `digits' consumed; make it negative to indicate
     * overflow.
     */
    cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
    cutlim = cutoff % (unsigned long)base;
    cutoff /= (unsigned long)base;
    for (acc = 0, any = 0;; c = *s++) {
	if (c >= '0' && c <= '9')
	    c -= '0';
	else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
	    c -= (c >= 'A' && c <= 'Z') ? 'A' - 10 : 'a' - 10;
	else
	    break;
	if (c >= base)
	    break;
	if (any < 0 || acc > cutoff || acc == cutoff && c > cutlim)
	    any = -1;
	else {
	    any = 1;
	    acc *= base;
	    acc += c;
	}
    }
    if (any < 0) {
	acc = neg ? LONG_MIN : LONG_MAX;
    } else if (neg)
	acc = -acc;
    if (endptr != NULL)
	*endptr = (char *)(any ? s - 1 : nptr);
    return (acc);
}

