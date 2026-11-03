#include "linkqueue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

QNode_t *CreateLinkQueue(void)
{
	QNode_t *pTmpNode = NULL;

	pTmpNode = malloc(sizeof(QNode_t));
	if (NULL == pTmpNode)
	{
		printf("malloc failed\n");
		return NULL;
	}

	pTmpNode->pNext = NULL;

	return pTmpNode;
}

int IsEmptyLinkQueue(QNode_t *pTmpQueue)
{
	return pTmpQueue->pNext == NULL ? 1 : 0;
}

int EnterLinkQueue(QNode_t *pTmpQueue, QDataType TmpData)
{
	QNode_t *pTmpNode = NULL;
	QNode_t *pLastNode = NULL;

	pTmpNode = malloc(sizeof(QNode_t));
	if (NULL == pTmpNode)
	{
		printf("malloc failed\n");
		return -1;
	}

	pTmpNode->Data = TmpData;
	pTmpNode->pNext = NULL;

	pLastNode = pTmpQueue;
	while (pLastNode->pNext != NULL)
	{
		pLastNode = pLastNode->pNext;
	}

	pLastNode->pNext = pTmpNode;

	return 0;
}
int PopLinkQueue(QNode_t *pTmpQueue, QDataType *pTmpData)
{
	QNode_t *pFreeNode = NULL;
	QDataType TmpData;

	if (IsEmptyLinkQueue(pTmpQueue))
	{
		return -1;
	}

	pFreeNode = pTmpQueue->pNext;
	pTmpQueue->pNext = pFreeNode->pNext;
    if (pTmpData != NULL)
    {
	    *pTmpData = pFreeNode->Data;
    }
	free(pFreeNode);
	pFreeNode = NULL;

	return 0;
}

int DestroyLinkQueue(QNode_t **ppTmpQueue)
{
	QNode_t *pTmpNode = NULL;
	QNode_t *pFreeNode = NULL;

	pTmpNode = pFreeNode = *ppTmpQueue;
	while (pTmpNode != NULL)
	{
		pTmpNode = pTmpNode->pNext;
		free(pFreeNode);
		pFreeNode = pTmpNode;
	}
	*ppTmpQueue = NULL;
	
	return 0;
}
