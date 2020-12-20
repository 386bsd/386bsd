
#define NSWSIZES	16	/* size of swtab */
#define NPENDINGIO	64	/* max # of pending cleans */
#define MAXDADDRS	64	/* max # of disk addrs for fixed allocations */

struct swpagerclean {
	queue_head_t		spc_list;
	int			spc_flags;
	struct buf		*spc_bp;
	sw_pager_t		spc_swp;
	vm_offset_t		spc_kva;
	vm_page_t		spc_m;
} swcleanlist[NPENDINGIO];
typedef	struct swpagerclean	*swp_clean_t;

/* spc_flags values */
#define SPC_FREE	0x00
#define SPC_BUSY	0x01
#define SPC_DONE	0x02
#define SPC_ERROR	0x04
#define SPC_DIRTY	0x08

struct swtab {
	vm_size_t st_osize;	/* size of object (bytes) */
	int	  st_bsize;	/* vs. size of swap block (DEV_BSIZE units) */
#ifdef DEBUG
	u_long	  st_inuse;	/* number in this range in use */
	u_long	  st_usecnt;	/* total used of this size */
#endif
} swtab[NSWSIZES+1];

#ifdef DEBUG
int		swap_pager_pendingio;	/* max pending async "clean" ops */
int		swap_pager_poip;	/* pageouts in progress */
int		swap_pager_piip;	/* pageins in progress */
#endif
