/*
   Copyright 2010, Kunihiko Sadakane, all rights reserved.

   This software may be used freely for any purpose.
   No warranty is given regarding the quality of this software.
*/
typedef struct dbwt_qblock {
  struct dbwt_qblock *prev, *next;
  packed_array *b;
} dbwt_qblock;

typedef struct {
  long n; // �v�f��
  int w; // �i�[����l�̃r�b�g��
  dbwt_qblock *sb, *eb;
  long s_ofs, e_ofs; // 0 <= s_ofs, e_ofs < QSIZ
} dbwt_queue;

dbwt_queue * dbwt_init_queue(int w);
void dbwt_enqueue(dbwt_queue *que, long x);
void dbwt_enqueue_l(dbwt_queue *que, long x);
long dbwt_dequeue(dbwt_queue *que);
int dbwt_emptyqueue(dbwt_queue *que);
void dbwt_free_queue(dbwt_queue *que);
void dbwt_printqueue(dbwt_queue *que);
