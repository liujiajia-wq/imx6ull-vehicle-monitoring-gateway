#include "linklist.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/***************************************
 *函数名:CreateEmptyLinkList
 *功能:
 *	创建一个包含头结点的空链表
 *参数:
 *	缺省
 *返回值:
 *	成功返回创建的空白节点的地址
 *	失败返回NULL
 **************************************/
Node_t *CreateEmptyLinkList(void)
{
	Node_t *pTmpNode = NULL;

	pTmpNode = malloc(sizeof(Node_t));
	if (NULL == pTmpNode)
	{
		printf("malloc failed\n");
		return NULL;
	}

	pTmpNode->pNext = NULL;
	
	return pTmpNode;
}

/***************************************
 *函数名:InsertHeadLinkList
 *功能:
 *	头插法
 *参数:
 *	pTmpList: 链表头结点地址 
 *	TmpData: 要插入的数据
 *返回值:
 *	成功返回0
 *	失败返回-1
 **************************************/
int InsertHeadLinkList(Node_t *pTmpList, DataType TmpData)
{
	Node_t *pTmpNode = NULL;

	//1.申请空间
	pTmpNode = malloc(sizeof(Node_t));
	if (NULL == pTmpNode)
	{
		printf("malloc failed\n");
		return -1;
	}

	//2.将data赋值
	pTmpNode->Data = TmpData;

	//3.将pNext赋值
	pTmpNode->pNext = pTmpList->pNext;

	//4.头节点的pNext指向申请的节点
	pTmpList->pNext = pTmpNode;
	
	return 0;
}

/***************************************
 *函数名:IsEmptyLinkList
 *功能:
 *	是否为空链表
 *参数:
 *	pTmpList: 链表头结点地址 
 *返回值:
 *	为空返回1
 *	不为空返回0 
 **************************************/
int IsEmptyLinkList(Node_t *pTmpList)
{
	return pTmpList->pNext == NULL ? 1 : 0;
}

/***************************************
 *函数名:ShowLinkList
 *功能:
 *	显示链表中所有元素
 *参数:
 *	pTmpList: 链表头结点地址 
 *返回值:
 *	缺省
 **************************************/
void ShowLinkList(Node_t *pTmpList)
{
	Node_t *pTmpNode = NULL;

	pTmpNode = pTmpList->pNext;
	while (pTmpNode != NULL)
	{
//		printf("%d ", pTmpNode->Data);
		pTmpNode = pTmpNode->pNext;
	}
	printf("\n");

	return;
}

/***************************************
 *函数名:ReplaceLinkList
 *功能:
 *	替换链表中的旧值更新为新值
 *参数:
 *	pTmpList: 链表头结点地址 
 *	OldData:旧数据
 *	NewData:新数据
 *返回值:
 *	成功返回 更新元素个数
 *	失败返回 -1
 **************************************/
int ReplaceLinkList(Node_t *pTmpList, DataType OldData, DataType NewData)
{
	Node_t *pTmpNode = NULL;
	int cnt = 0;

	pTmpNode = pTmpList->pNext;
	while (pTmpNode != NULL)
	{
		//if (pTmpNode->Data == OldData)
		{
			pTmpNode->Data = NewData;
			cnt++;
		}
		pTmpNode = pTmpNode->pNext;
	}
	
	return cnt;
}

/***************************************
 *函数名:FindLinkList
 *功能:
 *	在链表中查找指定元素
 *参数:
 *	pTmpList: 链表头结点地址 
 *	TmpData:要找的元素
 *返回值:
 *	成功返回找到元素节点的地址
 *	失败或者没有找到返回NULL
 **************************************/
Node_t *FindLinkList(Node_t *pTmpList, DataType TmpData)
{
	Node_t *pTmpNode = NULL;
	Node_t *pRetNode = NULL;

	pTmpNode = pTmpList->pNext;
	while (pTmpNode != NULL)
	{
	//	if (pTmpNode->Data == TmpData)
		{
			pRetNode = pTmpNode;
			break;
		}
		pTmpNode = pTmpNode->pNext;
	}

	return pRetNode;
}

/***************************************
 *函数名:DeleteLinkList
 *功能:
 *	删除链表中的元素
 *参数:
 *	pTmpList: 链表头结点地址 
 *	TmpData:要删除的元素
 *返回值:
 *	成功返回 删除元素的个数
 *	失败或者没有找到 0
 **************************************/
int DeleteLinkList(Node_t *pTmpList, DataType TmpData)
{
	Node_t *pTmpNode = NULL;
	Node_t *pPreNode = NULL;
	int cnt = 0;

	pTmpNode = pTmpList->pNext;
	pPreNode = pTmpList;

	while (pTmpNode != NULL)
	{
	//	if (pTmpNode->Data == TmpData)
		{
			pPreNode->pNext = pTmpNode->pNext;
			free(pTmpNode);
			pTmpNode = pPreNode->pNext;
			cnt++;
		}
	//	else 
		{
			pTmpNode = pTmpNode->pNext;
			pPreNode = pPreNode->pNext;
		}
	}

	return cnt;
}

/***************************************
 *函数名:DestroyLinkList
 *功能:
 *	销毁链表中所有节点元素
 *参数:
 *	pTmpList: 存放链表头结点的指针变量地址
 *返回值:
 *	缺省
 **************************************/
void DestroyLinkList(Node_t **ppTmpList)
{
	Node_t *pTmpNode = NULL;
	Node_t *pFreeNode = NULL;

	pTmpNode = pFreeNode = (*ppTmpList);
	while (pTmpNode != NULL)
	{
		pTmpNode = pTmpNode->pNext;
		free(pFreeNode);
		pFreeNode = pTmpNode;
	}

	*ppTmpList = NULL;

	return;
}

/***************************************
 *函数名:FindMidLinkList
 *功能:
 *	快速找到链表中间节点
 *参数:
 *	pTmpList: 存放链表头结点的指针变量地址
 *返回值:
 *	成功返回中间节点的地址
 **************************************/
Node_t *FindMidLinkList(Node_t *pTmpList)
{
	Node_t *pFast = NULL;
	Node_t *pSlow = NULL;

	pFast = pSlow = pTmpList->pNext;
	while (pFast != NULL)
	{
		pFast = pFast->pNext;
		if (NULL == pFast)
		{
			break;
		}
		pFast = pFast->pNext;
		pSlow = pSlow->pNext;
	}

	return pSlow;
}

/***************************************
 *函数名:FindLastKthLinkList
 *功能:
 *	快速找到链表倒数第k个节点
 *参数:
 *	pTmpList: 存放链表头结点的指针变量地址
 *	Kth:倒数第几个节点
 *返回值:
 *	成功返回节点的地址
 **************************************/
Node_t *FindLastKthLinkList(Node_t *pTmpList, int Kth)
{
	Node_t *pFast = NULL;
	Node_t *pSlow = NULL;
	int i = 0;

	pFast = pSlow = pTmpList->pNext;
	
	for (i = 0; i < Kth && pFast != NULL; i++)
	{
		pFast = pFast->pNext;
	}

	if (NULL == pFast)
	{
		return NULL;
	}
	
	while (pFast != NULL)
	{
		pFast = pFast->pNext;
		pSlow = pSlow->pNext;
	}

	return pSlow;
}

/***************************************
 *函数名:DeleteNode
 *功能:
 *	已知链表中间节点地址删除中间节点
 *参数:
 *	pTmpNode: 要删除的数据节点
 *返回值:
 *	成功返回0
 **************************************/
int DeleteNode(Node_t *pTmpNode)
{
	Node_t *pFreeNode = NULL;

	pFreeNode = pTmpNode->pNext;
	pTmpNode->Data = pFreeNode->Data;
	pTmpNode->pNext = pFreeNode->pNext;
	free(pFreeNode);
	pFreeNode = NULL;

	return 0;
}	

/***************************************
 *函数名:ReverseLinkList
 *功能:
 *	单向链表完成倒置
 *参数:
 *	pTmpList: 链表头结点地址
 *返回值:
 *	成功返回0
 **************************************/
int ReverseLinkList(Node_t *pTmpList)
{
	Node_t *pTmpNode = NULL;
	Node_t *pInsertNode = NULL;

	pTmpNode = pTmpList->pNext;
	pTmpList->pNext = NULL;

	while (pTmpNode != NULL)
	{
		pInsertNode = pTmpNode;
		pTmpNode = pTmpNode->pNext;
		pInsertNode->pNext = pTmpList->pNext;
		pTmpList->pNext = pInsertNode;
	}

	return 0;
}

/***************************************
 *函数名:BubbleSortLinkList
 *功能:
 *	单向链表的冒泡排序
 *参数:
 *	pTmpList: 链表头结点地址
 *返回值:
 *	成功返回0
 **************************************/
int BubbleSortLinkList(Node_t *pTmpList)
{
	Node_t *pTmpNode1 = NULL;
	Node_t *pTmpNode2 = NULL;
	Node_t *pEnd = NULL;
	DataType TmpData;

	if (NULL == pTmpList->pNext || NULL == pTmpList->pNext->pNext)
	{
		return 0;
	}

	while (pEnd != pTmpList->pNext->pNext)
	{
		pTmpNode1 = pTmpList->pNext;
		pTmpNode2 = pTmpNode1->pNext;
		
		while (pTmpNode2 != pEnd)
		{
		//	if (pTmpNode1->Data > pTmpNode2->Data)
			{
				TmpData = pTmpNode1->Data;
				pTmpNode1->Data = pTmpNode2->Data;
				pTmpNode2->Data = TmpData;
			}
			pTmpNode1 = pTmpNode1->pNext;
			pTmpNode2 = pTmpNode2->pNext;
		}
		pEnd = pTmpNode1;
	}

	return 0;
}
