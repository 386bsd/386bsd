#define DST 257
#define SRC 258
#define HOST 259
#define GATEWAY 260
#define NET 261
#define PORT 262
#define LESS 263
#define GREATER 264
#define PROTO 265
#define BYTE 266
#define ARP 267
#define RARP 268
#define IP 269
#define TCP 270
#define UDP 271
#define ICMP 272
#define BROADCAST 273
#define NUM 274
#define ETHER 275
#define GEQ 276
#define LEQ 277
#define NEQ 278
#define ID 279
#define EID 280
#define HID 281
#define LSH 282
#define RSH 283
#define LEN 284
#define OR 285
#define AND 286
#define UMINUS 287
typedef union {
	int i;
	u_long h;
	u_char *e;
	char *s;
	struct stmt *stmt;
	struct block *blk;
	struct arth *a;
} YYSTYPE;
extern YYSTYPE yylval;
