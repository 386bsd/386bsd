extern void
rt_missmsg(int type, struct sockaddr *dst, struct sockaddr *gate,
	struct sockaddr *mask, struct sockaddr *src, int flags, int error);
/* static */ struct rtentry *rtalloc1(struct sockaddr *dst, int report);
