/* $Id: w_proto.h,v 1.1.4.6 2011/05/05 01:24:52 uehira Exp $ */

#ifndef _WPROTO_H_
#define _WPROTO_H_

/** prototypes **/
void get_time(int []);
uint32_w mkuint4(const uint8_w *);
uint16_w mkuint2(const uint8_w *);
int bcd_dec(int [], uint8_w *);
int dec_bcd(uint8_w *, int *);
void adj_time_m(int []);
void adj_time(int []);
void adj_sec(int *, double *, int *, double *);
int time_cmp(int *, int *, int);
WIN_bs winform(int32_w *, uint8_w *, WIN_sr, WIN_ch);
uint32_w win2fix(uint8_w *, int32_w *, WIN_ch *, WIN_sr *);
int strncmp2(char *, char *, int);
int strcmp2(char *, char *);
WIN_bs read_onesec_win(FILE *, uint8_w **, size_t *);
WIN_bs read_onesec_win2(FILE *, uint8_w **, uint8_w **, size_t *);
void Shm_init(struct Shm *, size_t);
struct Shm * Shm_read_offline(key_t);
struct Shm * Shm_create_offline(key_t, size_t);
void WIN_version(void);
uint32_w win_chheader_info(const uint8_w *, WIN_ch *, WIN_sr *, int *);
uint32_w win_get_chhdr(const uint8_w *, WIN_ch *, WIN_sr *);
uint32_w get_sysch(const uint8_w *, WIN_ch *);
uint32_w get_sysch_mon(const uint8_w *, WIN_ch *);
void get_mon(WIN_sr, int32_w *, int32_w (*)[]);
uint8_w * compress_mon(int32_w *, uint8_w *);
void make_mon(uint8_w *, uint8_w *);
void t_bcd(time_t, uint8_w *);
time_t bcd_t(uint8_w *);
void time2bcd(time_t, uint8_w *);
time_t bcd2time(uint8_w *);
int time_cmpq(const void *, const void *);
int ch_cmpq(const void *, const void *);
void rmemo5(char [], int []);
void rmemo6(char [], int []);
int wmemo5(char [], int []);
int ** i_matrix(int, int);
WIN_bs get_merge_data(uint8_w *, uint8_w *, WIN_bs *, uint8_w *, WIN_bs *);
WIN_ch get_sysch_list(uint8_w *, WIN_bs, WIN_ch []);
WIN_ch get_chlist_chfile(FILE *, WIN_ch []);
WIN_bs get_select_data(uint8_w *, WIN_ch [], WIN_ch, uint8_w *, WIN_bs);
int WIN_time_hani(char [], int [], int []);
int read_channel_file(FILE *, struct channel_tbl [], int);
void str2double(char *, int, int, double *);
time_t shift_sec(uint8_w *, int);
int read_param_line(FILE *, char [], int);
int dir_check(char *);
time_t check_ts(uint8_w *, time_t, time_t);
size_t FinalB_read(struct FinalB *, FILE *);
size_t FinalB_write(struct FinalB, FILE *);

/* win_xmalloc etc. */
void * win_xmalloc(size_t);
void * win_xrealloc(void *, size_t);
void * win_xcalloc(size_t, size_t);
void win_xfree(void *);

/* winlib_log.c */
int find_oldest(char *, char *);
struct Shm * Shm_read(key_t, char *);
struct Shm * Shm_create(key_t, size_t, char *);
sa_family_t sockfd_to_family(int);

#endif  /* !_WPROTO_H_*/
