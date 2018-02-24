/*
   Copyright 2010, Kunihiko Sadakane, all rights reserved.

   This software may be used freely for any purpose.
   No warranty is given regarding the quality of this software.
*/
#include <stdio.h>
#include <stdlib.h>
#include "dbwt_utils.h"
#include "dbwt_queue.h"

#define QSIZ 1024

dbwt_queue * dbwt_init_queue(int w)
{
  dbwt_queue *que;

  que = (dbwt_queue *) dbwt_mymalloc(sizeof(dbwt_queue));
  que->n = 0;
  que->w = w;
  que->sb = que->eb = NULL;
  que->s_ofs = 0;
  que->e_ofs = QSIZ-1;
  return que;
}



void dbwt_enqueue(dbwt_queue *que, long x)
{
  dbwt_qblock *qb;
  if (que->e_ofs == QSIZ-1) { // ���݂̃u���b�N����t
    qb = (dbwt_qblock *) dbwt_mymalloc(sizeof(dbwt_qblock));
    qb->b = dbwt_allocate_packed_array(QSIZ,que->w);
    if (que->eb == NULL) { // �u���b�N��1���Ȃ�
      que->sb = que->eb = qb;
      que->s_ofs = 0;
      qb->prev = qb->next = NULL;
    } else {
      que->eb->next = qb; // ���݂̍Ō�̃u���b�N�̎��ɒǉ�
      qb->prev = que->eb;
      qb->next = NULL;
      que->eb = qb;
    }
    que->e_ofs = -1;
  }
  dbwt_pa_set(que->eb->b, ++que->e_ofs, x);
  que->n++;
}

void dbwt_enqueue_l(dbwt_queue *que, long x) // �擪�ɒǉ�
{
  dbwt_qblock *qb;
  if (que->s_ofs == 0) { // ���݂̃u���b�N����t
    qb = (dbwt_qblock *) dbwt_mymalloc(sizeof(dbwt_qblock));
    qb->b = dbwt_allocate_packed_array(QSIZ,que->w);
    if (que->sb == NULL) { // �u���b�N��1���Ȃ�
      que->sb = que->eb = qb;
      que->e_ofs = QSIZ-1;
      qb->prev = qb->next = NULL;
    } else {
      que->sb->prev = qb; // ���݂̍ŏ��̃u���b�N�̑O�ɒǉ�
      qb->next = que->sb;
      qb->prev = NULL;
      que->sb = qb;
    }
    que->s_ofs = QSIZ;
  }
  dbwt_pa_set(que->sb->b, --que->s_ofs, x);
  que->n++;
}

long dbwt_dequeue(dbwt_queue *que)
{
  long x;
  dbwt_qblock *qb;
  x = dbwt_pa_get(que->sb->b, que->s_ofs++);
  if (que->s_ofs == QSIZ) { // ���݂̃u���b�N�����
    qb = que->sb;           // ���݂̐擪�u���b�N
    que->sb = qb->next;
    dbwt_free_packed_array(qb->b);
    dbwt_myfree(qb,sizeof(dbwt_qblock));
    if (que->sb == NULL) { // �u���b�N�������Ȃ���
      que->eb = NULL;
      que->e_ofs = QSIZ-1;
      que->s_ofs = 0;
    } else {
      que->sb->prev = NULL;
      que->s_ofs = 0;
    }
  }
  que->n--;
  return x;
}

long dbwt_dequeue_r(dbwt_queue *que) // �Ōォ��폜
{
  long x;
  dbwt_qblock *qb;
  x = dbwt_pa_get(que->eb->b, que->e_ofs--);
  if (que->e_ofs < 0) { // ���݂̃u���b�N�����
    qb = que->eb;           // ���݂̍ŏI�u���b�N
    que->eb = qb->prev;
    dbwt_free_packed_array(qb->b);
    dbwt_myfree(qb,sizeof(dbwt_qblock));
    if (que->eb == NULL) { // �u���b�N�������Ȃ���
      que->sb = NULL;
      que->e_ofs = QSIZ-1;
      que->s_ofs = 0;
    } else {
      que->eb->next = NULL;
      que->e_ofs = QSIZ-1;
    }
  }
  que->n--;
  return x;
}

int dbwt_emptyqueue(dbwt_queue *que)
{
  return (que->n == 0);
}

void dbwt_free_queue(dbwt_queue *que)
{
  dbwt_qblock *qb,*q;
  qb = que->sb;
  while (qb != NULL) {
    q = qb->next;
    dbwt_free_packed_array(qb->b);
    dbwt_myfree(qb,sizeof(dbwt_qblock));
    qb = q;
  }
  dbwt_myfree(que,sizeof(*que));
}

void dbwt_printqueue( dbwt_queue *que)
{
  long i,s,t;
  dbwt_qblock *qb;
  printf("printqueue que=%p\n",que);
  if (que == NULL) {
    printf("printqueue: que == NULL\n");
    return;
  }
  qb = que->sb;
  if (qb == NULL) {
    printf("printqueue: empty\n");
    return;
  }
  while (qb != NULL) {
    printf("qblock %p prev=%p next=%p (",qb,qb->prev,qb->next);
    s = 0;  t = QSIZ-1;
    if (qb == que->sb) s = que->s_ofs;
    if (qb == que->eb) t = que->e_ofs;
    for (i=s; i<=t; i++) {
      printf("%lu ", dbwt_pa_get(qb->b,i));
    }
    printf(")\n");
    qb = qb->next;
  }
  printf("end\n");
}

