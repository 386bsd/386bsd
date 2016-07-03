/*
 * Copyright (c) 1994 William Jolitz. All rights reserved.
 * Written by William Jolitz 6/94
 *
 * Redistribution and use in source and binary forms are freely permitted
 * provided that the above copyright notice and attribution and date of work
 * and this paragraph are duplicated in all such forms.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * 386BSD Kernel Software Modules:
 *
 * Besides the core kernel (itself a module), all other facilities of
 * the kernel are composed of independant modules. The modular kernel
 * is constructed on a framework built of the following five parts:
 *	bootstrap load of primary module(s).
 *	dynamic discovery and binding
 *	symbolic configuration
 *	independant kernel interfaces
 *	configuration utilty processes.
 *
 * This file describes the mechanisms incorporated in each module to allow
 * for module discovery, symbolic configuration, and kernel interface
 * definition and attachment. Other parts of the framework and implementation
 * reside within the core kernel, bootstrap, and system utilities.
 *
 * Each module is self-configuring collection of object files, so the core
 * kernel (and bootstrap) has only the responsibility to pass control to
 * each module to have it "wire" itself into the kernel for operation.
 * Modules only initialize themselves on configuration, if
 * partially or fully dependant on another module for operation, it must
 * discover the presence or absence of the other module solely when performing
 * the operation, and only via an existing kernel interface (however obtuse).
 * (No attempts at "caching" this operation is allowed, and no "references"
 * are asserted on the underlying module -- the operation is stateless).
 */

/*
 * Module Configuration
 */

/*
 * Module Class Types: These describe modules that must be (re)initialized.
 * Order of class intialization is dictated by kernel implementation only,
 * going from lowest level of abstraction (bus) to highest
 * (filesystems & system calls).
 */
enum modtype {
	__MODT_FIRST__,		/* */
	__MODT_ALL__,		/* (configure all) */
	MODT_BUS,		/* bus adaptor/motherboard driver */
	MODT_CONSOLE,		/* system console device driver */
	MODT_EXTDMOD,		/* extended module services */
	MODT_OLDMODT,		/* older version module configuration */
	MODT_DRIVER,		/* device driver */
	MODT_NETWORK, 		/* network protocol (or domain) */
	MODT_FILESYSTEM,	/* filesystem */
	MODT_LDISC,		/* terminal line discipline */
	MODT_SYSCALLS,		/* system calls */
	MODT_KERNEL,		/* extended kernel function */
	MODT_SLIP,		/* serial line ip  XXX */
	MODT_LOCALNET,		/* local net interface XXX */
	/* add more here */
	__MODT_LAST__,		/* */
};
typedef	enum modtype modtype_t;
#define MODT_ISVALID(md) ((md) > __MODT_FIRST__ && (md) < __MODT_LAST__)

/*
 * Modules are discovered via the configuration process, currently a "one
 * time only" operation. Effectively they are like manually invoked C++
 * static constructors, and can allocate memory and other global kernel
 * resources for the private use of the module.
 *
 * Associated with each interface in a module is a configuration function
 * named by this macro, e.g.
 * DRIVER_CONFIG() {
 * }
 */
struct modconfig {
	int	mod_signature;
	int	mod_version;
	modtype_t mod_type;
	void	(*mod_init)(void);
};

#define MODULE_SIGNATURE	35789143
#define MODULE_VERSION		19940705
#define	__MODULE_CONFIG__(name, type) \
	static void __ ## name ## _init__ (void); \
	static struct modconfig __ ## name ## __ = \
		{ MODULE_SIGNATURE, MODULE_VERSION, type , \
		__ ## name ## _init__ }; \
	static void __ ## name ## _init__ (void)

#define	BUS_MODCONFIG()		__MODULE_CONFIG__(bus, MODT_BUS)
#define	CONSOLE_MODCONFIG()	__MODULE_CONFIG__(console, MODT_CONSOLE)
#define	EXTDMOD_MODCONFIG()	__MODULE_CONFIG__(driver, MODT_EXTDMOD)
#define	DRIVER_MODCONFIG()	__MODULE_CONFIG__(driver, MODT_DRIVER)
#define	NETWORK_MODCONFIG()	__MODULE_CONFIG__(network, MODT_NETWORK)
#define	FILESYSTEM_MODCONFIG() __MODULE_CONFIG__(filesystem, MODT_FILESYSTEM)
#define	LDISC_MODCONFIG()	__MODULE_CONFIG__(ldisc, MODT_LDISC)
#define	SYSCALLS_MODCONFIG() __MODULE_CONFIG__(syscalls, MODT_SYSCALLS)
#define	KERNEL_MODCONFIG() __MODULE_CONFIG__(kernel, MODT_KERNEL)
#define	SLIP_MODCONFIG() __MODULE_CONFIG__(slip, MODT_SLIP)
#define	LOCALNET_MODCONFIG() __MODULE_CONFIG__(localnet, MODT_LOCALNET)
/* add more here */

/* kernel interface to initializing a class of modules */
void	modscaninit(modtype_t);


/*
 * Module Configuration:
 */

/*
 * Modules learn their properties and configuration details from an embedded
 * string in the module, which is written in a trivial configuration language.
 * These details serve only to bootstrap the configuration process to the
 * point where conflicts can be resolved by user-level administration and
 * configuration code, which it is not a replacement for (as much as possible,
 * modules must manage resources dynamically and when needed).  These options
 * can be overridden in the kernels' configuration file, which is only used
 * to handle exceptional cases (like when debugging hardware/software conflicts,
 * or when testing replacement modules). While the drivers can be adjusted
 * with the configuration file to an arbitary degree, it is sensible to
 * only have the minimum number of non-standard configuration entries at any
 * time, otherwise the configuration file becomes its own problem.
 *
 * Order of evaluation of configuration within a class is intentionally not
 * implemented; modules should be written only to depend on the kernel, and
 * the kernel itself resolves all successive initializations necessary for
 * user-level interaction. In turn, user-level programs are responsible for
 * final configuration.
 *
 * The configuration implementation has a set of functions to return the
 * contents of the configuration as basic data types. For convience, the
 * "namelist" is present to fetch a series of named configurable variables.
 */

enum cfg_type { NUMBER, STRING, INET_ADDR };
typedef enum cfg_type cfg_type_t;

/*
 * List of unordered parameters, e.g.:
 *	<namelist> ::= <idstring> <thing> [<namelist>] ;
 */
struct namelist {
	char *name;		/* identifier string (e.g. maxusers) */
	void *value;		/* place to return a value */
	cfg_type_t type;	/* type of value to be parsed */
};

/* functions to evaluate configuration */
int config_scan(char *drv_config, char **cfg); 
char *cfg_skipwhite(char **cfg);
int cfg_number(char **cfg, int *pval);
int cfg_string(char **cfg, char *sp, int szval);
int cfg_char(char **cfg, char t);
int cfg_namelist(char **cfg, struct namelist *nmp);

/*
 * Module to Kernel, and Kernel to Module Interfaces:
 */

/*
 * Modules attach themselves via interfaces to the kernel. This is the only
 * way that they "export" functions. Modules can only export data abstractions
 * via explicit interaction with the kernel, and must always have a way of
 * releasing then upon demand, as the modules can be loaded/unloaded at any
 * time (unimplemented). It is up to the kernel (and its subsystems) to
 * regulate/compensate for this, as modules hold no state of permanance.
 *
 * Modules can selectively "import" kernel functions, provided that this
 * does not interfer with the kernel's stateless view of modules.
 *
 * Software configuration may trigger hardware configuration, which is
 * independant of the kernel and relative to the bus drivers.
 *
 * Each interface is described by an instance embedded in a module that
 * is registered with the kernel by means of a configuration function
 * called during module configuration. Numerous interfaces may be present.
 */

/*
 * [BSD SVR V7 ...] UN*X device-independant driver interface.
 *
 * This interface is an easily extended mechanism meant to allow
 * interface of historical device drivers. Native 386BSD drivers
 * have a "devif" embedded in them that is automatically configured
 * when the driver is loaded to join the kernel with the driver, while
 * foreign drivers are encapsulated in a 386BSD interface conversion
 * function which acts as a proxy, containing the "devif" and handling
 * configuration. The conversion function is compiled with the foreign
 * driver, along with a local simulated environment, to yield a 386BSD
 * compatable driver.
 */

/* device driver interface instance, defined in a driver */
struct devif {
	char di_name[32];		/* device driver name */
	int di_cmajor, di_bmajor;	/* block/character indexes */
	int di_minor_unit_mask;		/* minor device unit portion (unused) */
	int di_minor_unit_shift;
	int di_minor_subunit_mask;	/* minor device subunit portion (unused) */
	int di_minor_subunit_shift;
	int di_flags;			/* per driver flags (unused) */
	void *di_private;		/* per driver internal data (unused) */

	/* driver methods: all device kinds */
	int (*di_open)(dev_t dev, int oflags, int devtype, struct proc *p);
	int (*di_close)(dev_t dev, int fflag, int devtype, struct proc *p);
	int (*di_ioctl)(dev_t dev, int cmd, caddr_t data, int fflag, struct proc *p);

	/* driver methods: BSD Un*x character device kind */
	int (*di_read)	(dev_t dev, struct uio *uio, int ioflag);
	int (*di_write)	(dev_t dev, struct uio *uio, int ioflag);
	int (*di_select)(dev_t dev, int which, struct proc *p);
	int (*di_mmap)(dev_t dev, int offs, int nprot);	/* mapdev */

	/* driver methods: BSD Un*x block device kind */
	int (*di_strategy)(struct buf *bp);
	int (*di_getbuf)(struct buf *bp);
	int (*di_dump)(dev_t dev);
	int (*di_psize)(dev_t dev);

	/* driver methods: BSD Un*x console device */
	int (*di_getchar)(dev_t dev);
	void (*di_putchar)(dev_t dev, unsigned ch);

	/* driver methods: System V streams kind */
#ifdef notyet
	/* struct streamtab *di_stab; */
#else
	int	xxx_placeholder;
#endif

	/* 386BSD release 2 new driver methods: */
	int (*di_wcread)	(dev_t dev, struct uio *uio, int ioflag);
	int (*di_wcwrite)	(dev_t dev, struct uio *uio, int ioflag);
	caddr_t (*di_pagealloc)	(dev_t dev, int size);
	int (*di_pageread)	(dev_t dev, struct uio *uio, int ioflag);
	int (*di_pagewrite)	(dev_t dev, struct uio *uio, int ioflag);

	int di_reserved[4];	/* reserved for compatibility */
	
	/* additional methods, as needed at your pleasure */
};

/*
 * Driver Module to Kernel functions:
 */
int devif_config(char **cfg, struct devif *dif);

/*
 * Kernel interface to the device drivers via the devif interface.
 */

/* kinds of interfaces implemented within an devif interface */
enum devif_type { CHRDEV, BLKDEV, WCHRDEV, PAGEDEV, STRDEV };
typedef enum devif_type devif_type_t;

int devif_open(dev_t dev, devif_type_t typ, int flag, struct proc *p);
int devif_close(dev_t dev, devif_type_t typ, int flag, struct proc *p);
int devif_ioctl(dev_t dev, devif_type_t typ, int cmd, caddr_t data, int flag,
    struct proc *p);

int devif_read(dev_t dev, devif_type_t typ, struct uio *uio, int flag);
int devif_write(dev_t dev, devif_type_t typ, struct uio *uio, int flag);
int devif_select(dev_t dev, devif_type_t typ, int rw, struct proc *p);

int devif_strategy(devif_type_t typ, struct buf *bp);

int devif_mmap(dev_t dev, devif_type_t typ, int offset, int nprot);
int devif_psize(dev_t dev, devif_type_t typ);
int devif_dump(dev_t dev, devif_type_t typ);

int devif_cnget(dev_t dev, devif_type_t typ);
int devif_cnput(dev_t dev, devif_type_t typ, int c);
struct devif *devif_probe(char *name);
struct devif *devif_probedev(dev_t dev, devif_type_t typ);

/*
 * [BSD SVR V7 ...] UN*X line discipline interface.
 *
 * Line disciplines are a specific example of a "class" or super
 * driver, which implements a mode of operation over a group of
 * devices for a dedicated purpose. Generalized, stackable line
 * disciplines evolved into the AT&T Streams mechanism, from the
 * terminal driver concept they started out as in V4. These are
 * currently implemented only to allow historical code that has
 * yet to be reimplemented in an abstract class driver (since they
 * are not yet ready...), these include the terminal drivers, SLIP,
 * and a variety of existing packages (like ppp).
 */

/* Un*x tty line discipline interface instance, defined in module */
struct ldiscif {
	char li_name[32];		/* line disc name */
	int li_disc;			/* index / line disc number */
	int li_flags;			/* per line disc flags (unused) */
	void *li_private;		/* per driver internal data (unused) */
	struct ldiscif *li_next;	/* next line discipline in list */

	/* line disc methods: BSD Un*x disciplines */
	int (*li_open) (dev_t dev, struct tty *tp, int flag);
	void (*li_close) (struct tty *tp, int flag);
	int (*li_read) (struct tty *tp, struct uio *uio, int flag);
	int (*li_write) (struct tty *tp, struct uio *uio, int flag);
	int (*li_ioctl) (struct tty *tp, int cmd, caddr_t data, int flag, struct proc *p);
	void (*li_rint) (unsigned ch, struct tty *tp);
	int (*li_put) (unsigned ch, struct tty *tp);
	void (*li_start) (struct tty *tp);
	int (*li_modem) (struct tty *tp, int flag);
	int (*li_qsize) (struct tty *tp, void *q);
};

/*
 * Driver Module to Kernel functions:
 */
int ldiscif_config(char **cfg, struct ldiscif *lif);

/*
 * Kernel interface to the line discipline drivers via the ldiscif interface.
 */
int ldiscif_open(dev_t, struct tty *, int);
void ldiscif_close(struct tty *, int);
int ldiscif_read(struct tty *, struct uio *, int);
int ldiscif_write(struct tty *, struct uio *, int);
int ldiscif_ioctl(struct tty *, int, caddr_t, int, struct proc *);
void ldiscif_rint(unsigned, struct tty *);
unsigned ldiscif_put(unsigned, struct tty *);
void ldiscif_start(struct tty *);
int ldiscif_modem(struct tty *, int);
struct ldiscif *ldiscif_probe(char *name);

/*
 * Console Interface.
 */
int console_config(char **cons_cfg, char **mod_cfg, struct devif *dif);
void console_putchar(unsigned c);
unsigned console_getchar(void);
