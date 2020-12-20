/* extern.h - External declarations for cpio.  Requires system.h.
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

enum archive_format
{
  arf_unknown, arf_binary, arf_oldascii, arf_newascii, arf_crcascii,
  arf_tar, arf_ustar, arf_hpoldascii, arf_hpbinary
};
extern enum archive_format archive_format;
extern int reset_time_flag;
extern int io_block_size;
extern int create_dir_flag;
extern int rename_flag;
extern int table_flag;
extern int unconditional_flag;
extern int verbose_flag;
extern int dot_flag;
extern int link_flag;
extern int retain_time_flag;
extern int crc_i_flag;
extern int append_flag;
extern int swap_bytes_flag;
extern int swap_halfwords_flag;
extern int swapping_bytes;
extern int swapping_halfwords;
extern int set_owner_flag;
extern uid_t set_owner;
extern int set_group_flag;
extern gid_t set_group;
extern int no_chown_flag;
extern int last_header_start;
extern int copy_matching_files;
extern int numeric_uid;
extern char *pattern_file_name;
extern char *new_media_message;
extern char *new_media_message_with_number;
extern char *new_media_message_after_number;
extern int archive_des;
extern char *archive_name;
extern unsigned long crc;
#ifdef DEBUG_CPIO
extern int debug_flag;
#endif

extern char *input_buffer, *output_buffer;
extern char *in_buff, *out_buff;
extern long input_size, output_size;
extern long input_bytes, output_bytes;
extern char zeros_512[];
extern char *directory_name;
extern char **save_patterns;
extern int num_patterns;
extern char name_end;
extern char input_is_special;
extern char output_is_special;
extern char input_is_seekable;
extern char output_is_seekable;
extern int f_force_local;
extern char *program_name;
extern int (*xstat) ();
extern void (*copy_function) ();

#if __STDC__ || defined(__MSDOS__)
# define P_(s) s
#else
# define P_(s) ()
#endif

/* copyin.c */
void read_in_header P_((struct new_cpio_header *file_hdr, int in_des));
void read_in_old_ascii P_((struct new_cpio_header *file_hdr, int in_des));
void read_in_new_ascii P_((struct new_cpio_header *file_hdr, int in_des));
void read_in_binary P_((struct new_cpio_header *file_hdr, int in_des));
void swab_array P_((char *arg, int count));
void process_copy_in P_((void));
void long_format P_((struct new_cpio_header *file_hdr, char *link_name));
void print_name_with_quoting P_((char *p));

/* copyout.c */
void write_out_header P_((struct new_cpio_header *file_hdr, int out_des));
void process_copy_out P_((void));

/* copypass.c */
void process_copy_pass P_((void));

/* dirname.c */
char *dirname P_((char *path));

/* error.c */
void error P_((int status, int errnum, char *message, ...));

/* filemode.c */
void mode_string P_((unsigned int mode, char *str));

/* idcache.c */
#ifndef __MSDOS__
char *getgroup ();
char *getuser ();
uid_t *getuidbyname ();
gid_t *getgidbyname ();
#endif

/* main.c */
void process_args P_((int argc, char *argv[]));
void initialize_buffers P_((void));

/* makepath.c */
int make_path P_((char *argpath, int mode, int parent_mode,
		  uid_t owner, gid_t group, char *verbose_fmt_string));

/* stripslash.c */
void strip_trailing_slashes P_((char *path));

/* tar.c */
void write_out_tar_header P_((struct new_cpio_header *file_hdr, int out_des));
int null_block P_((long *block, int size));
void read_in_tar_header P_((struct new_cpio_header *file_hdr, int in_des));
int otoa P_((char *s, unsigned long *n));
int is_tar_header P_((char *buf));
int is_tar_filename_too_long P_((char *name));

/* userspec.c */
#ifndef __MSDOS__
char *parse_user_spec P_((char *name, uid_t *uid, gid_t *gid,
			  char **username, char **groupname));
#endif

/* util.c */
void empty_output_buffer P_((int out_des));
void swahw_array P_((char *ptr, int count));
void fill_input_buffer P_((int in_des, int num_bytes));
void copy_buf_out P_((char *in_buf, int out_des, long num_bytes));
void copy_in_buf P_((char *in_buf, int in_des, long num_bytes));
int peek_in_buf P_((char *peek_buf, int in_des, int num_bytes));
void toss_input P_((int in_des, long num_bytes));
void copy_files P_((int in_des, int out_des, long num_bytes));
void create_all_directories P_((char *name));
void prepare_append P_((int out_file_des));
char *find_inode_file P_((unsigned long node_num,
			  unsigned long major_num, unsigned long minor_num));
void add_inode P_((unsigned long node_num, char *file_name,
		   unsigned long major_num, unsigned long minor_num));
int open_archive P_((char *file));
void tape_offline P_((int tape_des));
void get_next_reel P_((int tape_des));
void set_new_media_message P_((char *message));
#ifdef __MSDOS__
int chown P_((char *path, int owner, int group));
#endif
#ifdef __TURBOC__
int utime P_((char *filename, struct utimbuf *utb));
#endif
#ifdef HPUX_CDF
char *add_cdf_double_slashes P_((char *filename));
#endif

/* xmalloc.c */
char *xmalloc P_((unsigned n));
char *xrealloc P_((char *p, unsigned n));

/* xstrdup.c */
char *xstrdup P_((char *string));
