#include <stdio.h>
#include <stdlib.h>
#include "api.h"
#include "network_ip.h"
#include "routing_wbca.h"
#include"math.h"
#include"mobility.h"
#include"node.h"
#include"Gui.h"
#include"fileio.h"
#include "propagation.h"
#include"main.h"
#include"trace.h"
#define WBCA_PC_ERAND(wbcaJitterSeed) (RANDOM_nrand(wbcaJitterSeed)\
    % WBCA_BROADCAST_JITTER)


const int DEBUG_MODE = 0x0E;

static
void WbcaInitializeConfigurableParameters(
    Node* node,
    const NodeInput* nodeInput,
    WbcaData* wbca,
    Address interfaceAddress)
{
}
static void //inline//
PrintStats(Node *node);
	
	static
	void WbcaSetTimer(
			 Node* node,
			 int eventType,
			 Address destAddr)
	{
		Message* newMsg = NULL;
		Address* info = NULL;
		NetworkRoutingProtocolType protocolType;
	
		protocolType = ROUTING_PROTOCOL_WBCA;
	
		// Allocate message for the timer
		newMsg = MESSAGE_Alloc(
					 node,
					 NETWORK_LAYER,
					 ROUTING_PROTOCOL_WBCA,
					 MSG_WBCA_SendHello);
		clocktype delay = 0.1* SECOND;
	
		// Assign the address for which the timer is meant for
		MESSAGE_InfoAlloc(
			node,
			newMsg,
			sizeof(Address));
	
		info = (Address *) MESSAGE_ReturnInfo(newMsg);
	
		memcpy(info, &destAddr, sizeof(Address));
		
		MESSAGE_Send(node, newMsg, delay);    
	}

void insertTraceState(Node* node,int state)
{
	node->tracestate=state;
}

//wbca³õÊ¼»¯
void
WbcaInit(
    Node* node,
    WbcaData** wbcaPtr,
    const NodeInput* nodeInput,
    int interfaceIndex,
    NetworkRoutingProtocolType wbcaProtocolType)
{  
	//printf("\n\n\nWBCA INIT\n\n\n");
    NetworkDataIp *ip = (NetworkDataIp *) node->networkData.networkVar;

    WbcaData* wbca = (WbcaData *) MEM_malloc(sizeof(WbcaData));

    BOOL retVal;
    char buf[MAX_STRING_LENGTH];//?
    int i = 0;
    Address destAddr;
    //NetworkRoutingProtocolType protocolType;

    (*wbcaPtr) = wbca;
    memset(wbca, 0, sizeof(WbcaData));
	

    wbca->iface = (WbcaInterfaceInfo *) MEM_malloc(
                                            sizeof(WbcaInterfaceInfo)
                                            * node->numberInterfaces);
	memset(wbca->iface,0,sizeof(WbcaInterfaceInfo) * node->numberInterfaces);
	
	wbca->state = 1;
	insertTraceState(node,wbca->state);

	wbca->conn = 0;//½ÚµãµÄÁ¬½Ó¶È¼´½ÚµãÁÚ¾Ó½Úµã¸öÊý
	wbca->numOfMem = 0;
	wbca->numOfMN = 0;
	wbca->numOfRoute = 0;
	wbca->Mn = 1;//æ–‡æ¡£é‡Œé¢çš„Ms
	wbca->memIdSeed = 1;
	wbca->checkMemNum = 0;
    wbca->x = node->mobilityData->current->position.common.c1;
    wbca->y = node->mobilityData->current->position.common.c2;
    wbca->z = node->mobilityData->current->position.common.c3;
    wbca->M=0;
	wbca->change=0;
	wbca->seqNumber=0;
	wbca->CR=0;
	wbca->statsPrinted=FALSE;


	for(int i=0; i<WBCA_MNLIST_SIZE; i++)
	{
		wbca->mnlist[i] = new WbcaMNList();
	}

	for(int j=0;j<WBCA_ROUTE_HASH_TABLE_SIZE;j++)
    {
   		wbca->routeTable[j] = new WbcaRouteTable();
    }

    NetworkIpSetRouterFunction(
        node,
        &Wbca4RouterFunction,
        interfaceIndex); 

    destAddr.networkType = NETWORK_IPV4;
    destAddr.interfaceAddr.ipv4 = ANY_DEST;
    //protocolType = ROUTING_PROTOCOL_WBCA;

    // Set default Interface Info
    wbca->defaultInterface = interfaceIndex;

    SetIPv4AddressInfo(
        &wbca->defaultInterfaceAddr,
        NetworkIpGetInterfaceAddress(node, interfaceIndex));
   
    wbca->statsCollected =TRUE;
    wbca->statsPrinted = FALSE;
   
	for (i = 0; i < node->numberInterfaces; i++)
    {
			wbca->iface[i].address.networkType = NETWORK_IPV4;
            wbca->iface[i].ip_version = NETWORK_IPV4;

            wbca->iface[i].address.interfaceAddr.ipv4 =
                        NetworkIpGetInterfaceAddress(node, i);

            wbca->iface[i].wbca4eligible = TRUE;
            wbca->iface[i].wbca6eligible = FALSE;
	}
	

	
    WbcaSetTimer(
        node,
        MSG_WBCA_SendHello,
        destAddr);

}


static BOOL
WbcaIsSmallerAddress(Address address1, Address address2)
{
	if (address1.networkType != address2.networkType)
	{
		ERROR_Assert(FALSE, "Address of same type not compared \n");
	}
	else if (address1.networkType == NETWORK_IPV6)
	{
		if (Ipv6CompareAddr6(
				address1.interfaceAddr.ipv6,
				address2.interfaceAddr.ipv6) < 0)
		{
			return TRUE;
		}
	}
	else
	{
		if (address1.interfaceAddr.ipv4 < address2.interfaceAddr.ipv4)
		{
			return TRUE;
		}
	}
	return FALSE;
}




//æš‚æ—¶ä¸ç”¨
// /**
// FUNCTION   :: WbcaIsEligibleInterface
// LAYER	  :: NETWORK
// PURPOSE	  :: Check whether interface is valid for WBCA for IPv4 or IPv6.
// PARAMETERS ::
//	+node:	Node* : Pointer to node.
//	+destAddr:	Address* : Pointer to Dest Address.
//	+iface:  WBCAInterfaceInfo* : Pointer to WBCA Interface.
// RETURN	  :: TRUE if Eligible, FALSE otherwise.
// **/
static
BOOL WbcaIsEligibleInterface(
	Node* node,
	Address* destAddr,
	WbcaInterfaceInfo* iface)
{
	if (destAddr->networkType == NETWORK_IPV4
		&& iface->wbca4eligible == TRUE
		|| destAddr->networkType == NETWORK_IPV6
		&& iface->wbca6eligible == TRUE
		)
	{
		return TRUE;
	}

	return FALSE;
}

//å‘æ¶ˆæ?
void
WbcaSendPacket(
	Node* node,
	Message* msg,
	Address srcAddr,
	Address destAddr,
	int interfaceIndex,
	int ttl,
	NodeAddress nextHopAddress,
	clocktype delay,
	BOOL isDelay)
{

	if(isDelay)
	{
		//Trace sending packet
		ActionData acnData;
		acnData.actionType = SEND;
		acnData.actionComment = NO_COMMENT;
		TRACE_PrintTrace(node, msg, TRACE_NETWORK_LAYER,
			  PACKET_OUT, &acnData , srcAddr.networkType);


		NetworkIpSendRawMessageToMacLayerWithDelay(
			node,
			msg,
			srcAddr.interfaceAddr.ipv4,
			destAddr.interfaceAddr.ipv4,
			IPTOS_PREC_INTERNETCONTROL,
			IPPROTO_WBCA,
			ttl,
			interfaceIndex,
			nextHopAddress,
			delay);

		//printf("LOG:WBCA HELLO PACKET FROM %x TO %x\n", srcAddr.interfaceAddr.ipv4, destAddr.interfaceAddr.ipv4);

			
	}
	else
	{
		//Trace sending packet
		ActionData acnData;
		acnData.actionType = SEND;
		acnData.actionComment = NO_COMMENT;
		TRACE_PrintTrace(node, msg, TRACE_NETWORK_LAYER,
				  PACKET_OUT, &acnData, srcAddr.networkType);

		//printf("LOG:WBCA MES PACKET FROM %d TO %d\n", node->nodeId, destAddr);

		NetworkIpSendRawMessageToMacLayer(
			node,
			msg,
			srcAddr.interfaceAddr.ipv4,
			destAddr.interfaceAddr.ipv4,
			IPTOS_PREC_INTERNETCONTROL,
			IPPROTO_WBCA,
			1,
			interfaceIndex,
			nextHopAddress);
	}

}

// /**
// FUNCTION   :: WbcaBroadcastHelloMessage
// LAYER	  :: NETWORK
// PURPOSE	  :: Function to advertise hello message if a node wants to.
// PARAMETERS ::
//	+node:	Node* : Pointer to node.
//	+aodv:	WBCAData* : Pointer to WBCA Data.
// RETURN	  :: void : NULL.
// **/
static
void WbcaBroadcastHelloMessage(Node* node, WbcaData* wbca, Address* destAddr)
{   
	Message* newMsg = NULL;
	WbcaHelloPacket* helloPkt = NULL;
	NetworkRoutingProtocolType protocolType = ROUTING_PROTOCOL_WBCA;
	char* pktPtr = NULL;
	int pktSize = sizeof(WbcaHelloPacket);
	int i= 0;
	UInt8 mesType = 0;//æ•°æ®åŒ…ç±»åž?
	BOOL isDelay = TRUE;
	Address broadcastAddress;

	newMsg = MESSAGE_Alloc(
				 node,
				 NETWORK_LAYER,
				 protocolType,
				 MSG_MAC_FromNetwork);
  
	MESSAGE_PacketAlloc(
		node,
		newMsg,
		pktSize,
		TRACE_WBCA);

	pktPtr = (char *) MESSAGE_ReturnPacket(newMsg);

	memset(pktPtr, 0, pktSize);


	// Section 8.4 of draft-ietf-manet-aodv-08.txt
	// Allocate the message and then broadcast to all interfaces
	// Hello Mesage 
	helloPkt = (WbcaHelloPacket *) pktPtr;
	helloPkt->CID = wbca->CID;
	helloPkt->conn = wbca->conn;
	helloPkt->Wei = wbca->wei;
	helloPkt->Mn = wbca->numOfMem;
	helloPkt->state = wbca->state;
	helloPkt->mesType = Wbca_Hello;
	helloPkt->sourceAddr = ANY_IP;
	helloPkt->destination.seqNum = wbca->seqNumber;
    helloPkt->x=wbca->x;
    helloPkt->y=wbca->y;
    helloPkt->z=wbca->z;
	for (i = 0; i < node->numberInterfaces; i++)
	{
		/*if (WbcaIsEligibleInterface(node, destAddr, &wbca->iface[i])
																== FALSE)
		{
			continue;
		}*/

		clocktype delay =
			(clocktype) WBCA_PC_ERAND(wbca->wbcaJitterSeed);

		helloPkt = (WbcaHelloPacket *) MESSAGE_ReturnPacket(newMsg);
		helloPkt->destination.address =
			wbca->iface[i].address.interfaceAddr.ipv4;
			
		SetIPv4AddressInfo(&broadcastAddress,ANY_DEST);
/*
		if (AODV_DEBUG_AODV_TRACE)
		{
			WBCAPrintTrace(node, newMsg, 'S',IPV6);
		}
*/

		WbcaSendPacket(
			node,
			MESSAGE_Duplicate(node, newMsg),
			wbca->iface[i].address,
			*destAddr,
			i,
			1,
			ANY_DEST,
			delay,
			isDelay);
	}
   

	MESSAGE_Free(node, newMsg);


}

//----------------------------------------------
//--------------------------------------------//----------------------------------------------
//--------------------------------------------
BOOL WbcaIPIsMyIp(Node* node,Address destAddr)
{
 return(NetworkIpIsMyIP(node, destAddr.interfaceAddr.ipv4));
}
WbcaMNList* WbcaCheckMNlistExist(Address destAddr,WbcaData* wbca,BOOL* isValidMN)

{
 WbcaMNList* current =NULL;
 for(Int8 i=0;i<wbca->numOfMN;i++)
 	{
 	if(destAddr.interfaceAddr.ipv4 == wbca->mnlist[i]->IP)
 		{
 		current = wbca->mnlist[i];
		break;
 		}
 	}
 if(current)
 	{
 	*isValidMN=TRUE;
	return current;
 	}
 else
 	{
 	*isValidMN=FALSE;
	return NULL;
 	}
}

WbcaRouteTable* WbcaCheckRouteExist(Address destAddr,WbcaData* wbca,BOOL* isValidR)

{
 WbcaRouteTable* current =NULL;
 for(Int8 i=0;i<wbca->numOfRoute;i++)
 	{
 	if(destAddr.interfaceAddr.ipv4 == wbca->routeTable[i]->destnation)
 		{
 		current = wbca->routeTable[i];
		break;
 		}
 	}
 if(current)
 	{
 	*isValidR=TRUE;
		return current;
 	}
 else
 	{
 	*isValidR=FALSE;
	return NULL;
 	}
}

//ä½¿ç”¨é‚»å±…è¡¨è½¬å‘æ•°æ®ï¼Œå¤šæ’­ 通过邻居表找到的路由,直接通过找到的邻居表项发过去
void wbcaTransmitDataMN(Node* node,Message* msg,WbcaMNList* rtEntryToDest)
{
	WbcaData* wbca=NULL;
	IpHeaderType* ipHeader=NULL;
	Address src;
	Address dest;
	wbca=(WbcaData*)NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_WBCA,NETWORK_IPV4);
	ipHeader=(IpHeaderType*)MESSAGE_ReturnPacket(msg);
	SetIPv4AddressInfo(&src, ipHeader->ip_src);
	SetIPv4AddressInfo(&dest, ipHeader->ip_dst);//?

	MESSAGE_SetLayer(msg, MAC_LAYER, 0);
	MESSAGE_SetEvent(msg, MSG_MAC_FromNetwork);
	for(Int8 i=0;i<node->numberInterfaces;i++)
	{
		NetworkIpSendPacketToMacLayer(node, msg, i, rtEntryToDest->IP);
	}

}
 //ä½¿ç”¨è·¯ç”±è¡¨è½¬å‘æ•°æ®ï¼Œå¤šæ’­
 void wbcaTransmitDataR(Node * node, Message * msg, WbcaRouteTable* rtEntryToDest)
 	{
 	WbcaData* wbca=NULL;
	IpHeaderType* ipHeader=NULL;
	Address src;
	Address dest;
	wbca =(WbcaData*)NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_WBCA,NETWORK_IPV4);
	ipHeader=(IpHeaderType*)MESSAGE_ReturnPacket(msg);
	SetIPv4AddressInfo(&src, ipHeader->ip_src);
	SetIPv4AddressInfo(&dest, ipHeader->ip_dst);
	MESSAGE_SetLayer(msg, MAC_LAYER, 0);
	MESSAGE_SetEvent(msg, MSG_MAC_FromNetwork);
	for(Int8 i=0;i< node->numberInterfaces;i++)
		{
		NetworkIpSendPacketToMacLayer(node, msg, i, rtEntryToDest->NextHop);
		}
 	}


//节点具体怎样处理来自某个节点的数据包,将这个包正确的接受\转发.或者说,怎样找到\建立路由
void WbcaHandleData(Node* node,Message* msg,Address destAddr)
{

	WbcaData* wbca=NULL;
	IpHeaderType* ipHeader=NULL;
	Address sourceAddress;
	wbca=(WbcaData*)NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_WBCA,NETWORK_IPV4);
	ipHeader = (IpHeaderType *)MESSAGE_ReturnPacket(msg);
	SetIPv4AddressInfo(&sourceAddress, ipHeader->ip_src);




	if(WbcaIPIsMyIp(node, destAddr)){ // 目的地是我自己
		return;
	}else{

			
		if(ipHeader->ip_p==0x11){
			printf("node %d Handle UDP Data from %x to %x\n",node->nodeId,ipHeader->ip_src,destAddr.interfaceAddr.ipv4);
		}
			


		WbcaRouteTable* rtToDestR=NULL; //通向目的节点所在簇头的路由表项
		WbcaMNList* rtToDestMN=NULL; //通向目的节点的邻居表项
		BOOL isValidR=FALSE; //是否可以通过路由表找到通路
		BOOL isValidMN=FALSE; //是否可以通过邻居表找到通路

		if(wbca->state==4||wbca->state==5){ //自己是簇头
			rtToDestMN=WbcaCheckMNlistExist(destAddr,wbca,&isValidMN);//检查目的是不是在邻居表中
			if(!(rtToDestMN && isValidMN)){
				rtToDestR=WbcaCheckRouteExist(destAddr,wbca,&isValidR);//检查目的是不是在路由表中
			}
			if(!((rtToDestMN && isValidMN)||(rtToDestR && isValidR))){//两种都不在的话

		    	Int16 headerCid=0;
				NodeAddress IP=destAddr.interfaceAddr.ipv4;
				headerCid=(IP-0xc0000000)*256; //拿到了目的节点的簇头CID
				for(Int8 i=0;i<wbca->numOfRoute;i++){
					if(headerCid == wbca->routeTable[i]->destCID){
						rtToDestR =wbca->routeTable[i];
						isValidR=TRUE;
						break;
					}
				}
				// if(node->nodeId==5&&ipHeader->ip_p==0x11&&destAddr.interfaceAddr.ipv4==0xa9000009){
				// 	rtToDestR =wbca->routeTable[1];
				// 	isValidR=TRUE;
				// }
		    }
	    }else{ //自己不是簇头
			rtToDestMN=WbcaCheckMNlistExist(destAddr, wbca, &isValidMN); //检查是不是在自己的邻居表中
			if(!(rtToDestMN&&isValidMN)){ //并没有通过邻居表直达的方法
				UInt16 headerCid=0;
				NodeAddress IP=destAddr.interfaceAddr.ipv4;
				headerCid=(IP-0xc0000000)*256;
				headerCid = wbca->CID/256*256;//取出自己的簇头cid;
				for(Int8 i=0;i<wbca->numOfMN;i++){
					if(headerCid == wbca->mnlist[i]->CID){
						rtToDestMN =wbca->mnlist[i];
						isValidMN=TRUE;
						break;
					}
				}
			}	
		}

		if(rtToDestMN){
			wbcaTransmitDataMN(node,msg,rtToDestMN);
		}
		else if(rtToDestR){
			wbcaTransmitDataR(node,msg,rtToDestR);
		}
		else{
			MESSAGE_Free(node,msg);
		}
	}
}
//----------------------------------------------
//--------------------------------------------
//----------------------------------------------
//--------------------------------------------





double CalMov(WbcaData *wbca,double x,double y,double z)
  {
  double s=sqrt((x-wbca->x)*(x-wbca->x)+(y-wbca->y)*(y-wbca->y));
//wbca->M+=s;
    return s;
  }
//è®¡ç®—æƒå€?
double
CalWei(WbcaData * wbca){
	double p=0;//average connectivity
	double c=0;//relative connectivity
	double CM=0;//relative mobo
	double sum=0;
	double ww;
   // CM=wbca->M;
	for(Int8 i=0; i<wbca->numOfMN; i++)
	{
		p = p + wbca->mnlist[i]->conn;
	}
	for(Int8 i=0; i<wbca->numOfMN; i++)//
	{
	
	//sum+=CalMov(wbca,wbca->mnlist[i]->x,wbca->mnlist[i]->y,wbca->mnlist[i]->z);
		sum+=wbca->mnlist[i]->m;
	}
	
	
	if(wbca->numOfMN != 0)
	{
		p = p / wbca->numOfMN;
        CM=sum/wbca->numOfMN;
		CM=CM/600; //guiyi
		wbca->M=CM;
	}
	else
	{
		p = 0;
	}

	c = wbca->conn - p;
	if(c < 0)
	{
		c = 0 - c;
	}
	//double cc = c / Wbca_Conn_To1;
	//printf("weight of 0x%x is %f\n",wbca->iface->address.interfaceAddr.ipv4,c);
	c=c/19;//guiyi
ww=c*0.5+CM*0.5;
//	printf("c=%f  : cm=%f\n",c,CM);
	return ww;



}

//delete from mnlist
void
DeleteFromMNList(WbcaData* wbca, Int8 del){
	Int8 i=0;
	for(i=0; i<wbca->numOfMN; i++)
	{
		if(del == i) 
		{
			break;
		}
	}
	free(wbca->mnlist[i]);
	while(i < wbca->numOfMN-1)
	{
		wbca->mnlist[i] = wbca->mnlist[i+1];
		i++;
	}
	
	wbca->mnlist[wbca->numOfMN - 1] = new WbcaMNList();
	wbca->numOfMN--;
}

//number of member
Int8
NumOfMem(WbcaData* wbca){
	Int8 i=0;
	Int8 j=0; 

	if(wbca->state != 4 && wbca->state != 5)
	{
		return 0;
	}
	else
	{
		for(i=0; i<wbca->numOfMN; i++)
		{
			if(i == wbca->numOfMN)
			{
				return j;
			}
			else
			{	
				if((wbca->mnlist[i]->CID)/256*256 == wbca->CID)
				{
					j++;
				}
			}
		}
	}

	wbca->numOfMem = j;

	return j;
	
}



BOOL
WbcaCheckMemID(WbcaData* wbca, UInt16 ID)
{
	for(int i=0; i<wbca->numOfMN; i++)
	{
		if(wbca->mnlist[i]->CID / 256 * 256 == wbca->CID)
		{
			if(wbca->mnlist[i]->CID % 256 == ID)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

void
WbcaInsertMemMNList(WbcaData* wbca, Node* node, NodeAddress ip, Int16 CID,double wei)
{
	bool isInMNList = FALSE;
	int i;
	BOOL isUn=0;
	BOOL isHeader=0;
	BOOL isMem=0;
    Int8 getMemId = CID % 256;
	Int8 getHeaderId = CID / 256;

	if(getMemId == 0 && getHeaderId == 0)
	{
			isUn = 1;
	}
	else if(getMemId == 0 && getHeaderId != 0)
	{
			isHeader = 1;
	}
	else
	{
			isMem = 1;
	}

	for(i=0; i<wbca->numOfMN; i++)
	{
		if(wbca->mnlist[i]->IP == ip)
		{
			isInMNList = TRUE;
			break;
		}
	}

	if(isInMNList)
	{
		//printf("isInMNList\n");
		wbca->mnlist[i]->CID = CID;
		wbca->mnlist[i]->wei = wei;
		if(isHeader)
			{
				wbca->mnlist[i]->okHeader=TRUE;
			}
		else
			{
				wbca->mnlist[i]->okHeader=FALSE;
			}
	}
	else
	{

		wbca->mnlist[wbca->numOfMN]->CID = CID;
		wbca->mnlist[wbca->numOfMN]->IP = ip;
		wbca->mnlist[wbca->numOfMN]->TTL = getSimTime(node);
		wbca->mnlist[i]->wei = wei;
		if(isHeader)
			{
			printf("ISHEADER\n");
			wbca->mnlist[i]->okHeader=TRUE;
			if(wbca->numOfRoute>0)
				{
					printf("wbca->numOfRoute\n");
			for(Int8 j=0;j<wbca->numOfRoute;j++)
				{
				if(wbca->routeTable[j]->nextCID==CID)
					{
					if(wbca->routeTable[j]->seqNum%2!=0)
						{
							printf("wbca->CR++\n");
						wbca->routeTable[j]->seqNum++;
						wbca->CR++;
						}
					}
				}
			}
			}
		else
			{
			wbca->mnlist[i]->okHeader=FALSE;
			}
		wbca->numOfMN++;
	}
}

void
WbcaCheckMNList(WbcaData * wbca, Node* node)
{
	bool flag=0;

	for(Int8 i=0; i<wbca->numOfMN; i++) //循环检查是否有邻居失联超过6T
	{
		//wbca->mnlist[i]->TTL = getSimTime(node);
		if((getSimTime(node) - wbca->mnlist[i]->TTL) >6 * WBCA_HELLO_INTERVAL)//和自己邻居6T没有联系到,有邻居失联
		{
			if(wbca->state == MEMBER && wbca->mnlist[i]->CID == (wbca->CID / 256 * 256)) //自己是成员丢失的是自己的簇头
			{
				if(DEBUG_MODE&0x04){
					printf("node %d cid(%x) MEMBER->PRE_MEMBER lost leader\n",node->nodeId,wbca->CID);
				}
				wbca->state = PRE_MEMBER;
				insertTraceState(node,wbca->state);
				wbca->CID = 0;
				wbca->cnt = 0;
				clocktype currtime=getSimTime(node);
					//---
		            GUI_SetNodeIcon(node->nodeId,
							"gui/icons/default.png",
							currtime);
			}

			//å½“è·¯ç”±è¡¨å»ºç«‹åŽå®Œæˆåºåˆ—å·åŠ 1
			if((wbca->numOfRoute>0 )&& (wbca->mnlist[i]->okHeader))//自己是簇头,丢失了自己邻居表中的另一位簇头
	        {
	          for(Int8 k=0;k<wbca->numOfRoute;k++)
		      {
		        if(wbca->routeTable[k]->nextCID==wbca->mnlist[i]->CID)
			     {
			      if(wbca->routeTable[k]->seqNum%2==0)//序列号是偶数的话
				    {
				     wbca->routeTable[k]->seqNum++;
				     wbca->routeTable[k]->distance=WBCA_Max;
				     wbca->CR++;
				    }
		         }
              }		
            }
			DeleteFromMNList(wbca, i);
		}
	}
	
	if(wbca->state == MEMBER)
	{
		for(Int8 i=0; i<wbca->numOfMN; i++)
		{
			if(wbca->mnlist[i]->CID == (wbca->CID / 256 * 256))
			{
				flag = 1;
			}
		}

		if(flag == 0)
		{
			wbca->state = COMPARING;
			wbca->CID = 0;
			clocktype currtime=getSimTime(node);
            GUI_SetNodeIcon(node->nodeId,
					"gui/icons/default.png",
					currtime);
			
		}
	}

	if(wbca->state == LEADER)
	{
		for(Int8 i=0; i<wbca->numOfMN; i++)
		{
			if((wbca->mnlist[i]->CID / 256 * 256) == wbca->CID)
			{
				flag = 1;
			}
		}

		if(flag == 0)
		{
			printf("node %d become lonely leader\n",node->nodeId);
			wbca->state = LONELY_LEADER;
			clocktype currtime=getSimTime(node);
            GUI_SetNodeIcon(node->nodeId,
					"gui/icons/tank.png",
					currtime);
		}
	}
}

void
WbcaUpdateMNList(WbcaData* wbca, Node* node, NodeAddress ip, Int16 CID, Int8 conn,double m, double wei)
{
	bool isInMnlist = 0;

	//循环判断 更新邻居表	
	for(Int8 i=0; i<wbca->numOfMN; i++)
	{	
		if(wbca->mnlist[i]->IP == ip)
		{
			wbca->mnlist[i]->TTL = getSimTime(node);
			wbca->mnlist[i]->CID = CID;
			wbca->mnlist[i]->conn = conn;
			wbca->mnlist[i]->wei = wei;
            wbca->mnlist[i]->m=m;
			isInMnlist = 1;
			
			break;
		}
	}

	//出现新的邻居,邻居表加一项
	if(! isInMnlist)
	{
		//temp = wbca->mnlist[wbca->numOfMN];
		wbca->mnlist[wbca->numOfMN]->TTL = getSimTime(node);
		wbca->mnlist[wbca->numOfMN]->CID = CID;
		wbca->mnlist[wbca->numOfMN]->conn = conn;
		wbca->mnlist[wbca->numOfMN]->wei = wei;
		wbca->mnlist[wbca->numOfMN]->IP = ip;	
		wbca->mnlist[wbca->numOfMN]->m=m;
        wbca->mnlist[wbca->numOfMN]->wei = wei;

		wbca->numOfMN++;
	}

}

void
WbcaOutputMNList(WbcaData *wbca)
{
	//printf("lingjubiao-----------------------------------0x%x  state:%d--------------------------------------\n",wbca->CID, wbca->state);
	for(int i=0; i< wbca->numOfMN; i++)
	{
		//printf("CID:0x%x   ###\n",wbca->mnlist[i]->CID);
		
	}
	//printf("\n");
}

void
WbcaOutputMNListMembers(WbcaData *wbca)
{
	if(wbca->state == 4 || wbca->state == 5)
	{
		int i = 0;
		int j = 0;
	    int k=0;
		//printf("cuneijiedian-----------------------------------0x%x  state:%d--------------------------------------\n",wbca->CID, wbca->state);
		for(i=0; i< wbca->numOfMN; i++)
		{
			if((wbca->mnlist[i]->CID/256*256) == wbca->CID)
			{
				//printf("CID:0x%x   ###\n",wbca->mnlist[i]->CID);
				j++;
				}
		}

		for(i=0; i< wbca->numOfMN; i++)
			{

		if(wbca->mnlist[i]->CID/10000!= wbca->CID/10000&&wbca->mnlist[i]->CID%10000==0)
			k++;



		}

	
		
		//printf("\n");
		//printf("number of neighbor : %d ; number of members : %d; mn is another %d\n",i,j,k);
	}

}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------

//insert and update the route table-----------
//增加或修改路由表的一条记录(如果已有则修改)
void 
WbcaInsertReplaceRouteTable(Node* node,
                                   WbcaData* wbca,
              		                NodeAddress destIp,
								   Int16 destCID,
								   NodeAddress nextIP,
								   Int16 nextCID,
								   Int16 distance,
								   UInt32 seqNum)
{
	//是簇头才有路由表
	if(wbca->state ==4 || wbca->state ==5){
		BOOL isRoute = FALSE;
		//int index = wbca->numOfRoute;
		int i;

		
		if(wbca->numOfRoute > 0){ //路由表成员个数大于0
			//printf("	wbca->numOfRoute!=0\n");
			//循环判断是不是需要更新表项
		    for(i = 0;i<wbca->numOfRoute;i++){
		    	//下面的if进入条件是有路由表记录需要更新
			 	if((wbca->routeTable[i]->destnation == destIp) && (wbca->routeTable[i]->destCID ==destCID)){
		 			//printf("		wbca->routeTable[i]->destnation == destIp) && (wbca->routeTable[i]->destCID ==destCID)\n");
			 		if(wbca->routeTable[i]->seqNum < seqNum){
			 				//printf("			wbca->routeTable[i]->seqNum < seqNum\n");
				 			wbca->routeTable[i]->NextHop = nextIP;
							wbca->routeTable[i]->nextCID = nextCID;
							wbca->routeTable[i]->distance = distance+1;
							wbca->routeTable[i]->seqNum = seqNum;
							wbca->CR++;
			 		}else if(wbca->routeTable[i]->seqNum ==seqNum){
						//printf("	wbca->routeTable[i]->seqNum ==seqNum\n");
						if(wbca->routeTable[i]->distance > distance+1){
							//printf("	wbca->routeTable[i]->distance > distance+1\n");
							wbca->routeTable[i]->NextHop = nextIP;
							wbca->routeTable[i]->nextCID = nextCID;
							wbca->routeTable[i]->distance = distance+1;
							//wbca->routeTable[i]->seqNum = seqNum;
							wbca->CR++;
					    }
					}
					isRoute =TRUE;	
		 		}
		 	}
		}

		//没有出现更新,新增表项
		if(!isRoute && wbca->numOfRoute < WBCA_ROUTE_HASH_TABLE_SIZE)
		{
			wbca->routeTable[wbca->numOfRoute]->destnation = destIp;
			wbca->routeTable[wbca->numOfRoute]->destCID = destCID;
			wbca->routeTable[wbca->numOfRoute]->NextHop = nextIP;
			wbca->routeTable[wbca->numOfRoute]->nextCID = nextCID;
			if(wbca->iface->address.interfaceAddr.ipv4 == destIp){
				wbca->routeTable[wbca->numOfRoute]->distance = distance;
			}else{
				wbca->routeTable[wbca->numOfRoute]->distance = distance+1;
			}
			wbca->routeTable[wbca->numOfRoute]->seqNum = seqNum;
			wbca->numOfRoute+=1;
			wbca->CR+=1;
	 	}
	/*
	if(wbca->numOfRoute <=0)
	{
	printf("the number of the route table:%d    Error:can't insert\upadate the route table!\n",wbca->numOfRoute);
	}
	else
	{
	printf("the number of the route table:%d\n",wbca->numOfRoute);
	for(int j = 0;j<wbca->numOfRoute;j++)
	{
	printf("%d : destination:0x%x  CID:0x%x  source:0x%x  CID:0x%x  distance:%d    seqNum:%d \n",j+1,wbca->routeTable[j]->destnation,wbca->routeTable[j]->destCID,wbca->routeTable[j]->NextHop,wbca->routeTable[j]->nextCID,wbca->routeTable[j]->distance,wbca->routeTable[j]->seqNum);
	}
	}
	*/
	 	//printf("插入自身节点后!WbcaInsertReplaceRouteTable wbca->CR: %d\n",wbca->CR);
	}
}

//output the route table---
void WbcaOutputRouteTable(WbcaData *wbca)
{
  if(wbca->state == 4 || wbca->state == 5)
  	{
  	int i;
	if(wbca->numOfRoute <= 0)
		{
		printf("the route is zero\n");
		}
	else
		{
		printf("Next is the 0x%x routable   state:%d",wbca->iface->address.interfaceAddr.ipv4,wbca->state);
		printf("\n");
	for(i = 0;i<wbca->numOfRoute;i++)
		{
		printf("destIP:0x%x  desCID:0x%x   nextIP:0x%x  nextCID:0x%x distance:%d seqNum:%d ######",wbca->routeTable[i]->destnation,wbca->routeTable[i]->destCID,wbca->routeTable[i]->NextHop,wbca->routeTable[i]->nextCID,wbca->routeTable[i]->distance,wbca->routeTable[i]->seqNum);		
		}
	    printf("\n");
		}
  	}
}
//å°†è·¯ç”±è¡¨æ•´ä½“å‘é€ï¼Œç¬¬ä¸€é¡¹ä¸ºç°‡å¤´è‡ªèº«ä¿¡æ¯ï¼Œå¤šæ’­çš„å½¢å¼
//构造并广播发送RTUP消息
void WbcaRTUPMessage(Node* node,WbcaData* wbca,Address* destAddr)
{
	int sizeRoute = wbca->numOfRoute;
	WbcaRTUP* RTUP=(WbcaRTUP*)MEM_malloc(sizeRoute*sizeof(WbcaRTUP));

	//循环遍历路由表,构造RTUP消息体
	for(int i=0;i<sizeRoute;i++){
		if(wbca->CID == wbca->routeTable[i]->destCID && wbca->routeTable[i]->distance==0){ 
			//如果是节点本身的路由项 //序列号+2
	    	wbca->routeTable[i]->seqNum+=2; 	
		}
		//把路由表信息填到RTUP消息中
		RTUP[i].destIP = wbca->routeTable[i]->destnation;
		RTUP[i].destCID = wbca->routeTable[i]->destCID;
		RTUP[i].distance = wbca->routeTable[i]->distance;
		RTUP[i].seqNum = wbca->routeTable[i]->seqNum; 
	}


	//暂时注释,本来是用来调试的代码
	if(sizeRoute <=0){
		printf("ERROR: when boardcast RTUP message node %d routecount <= 0 \n",node->nodeId);
	}else{
		printf("--DISPLAY Route Table of node %d before boardcast RTUP----\n",node->nodeId);
	    for(int j = 0;j<sizeRoute;j++){
	    	printf("\t NO\tdestIP\tdestCID\tnhopIP\tnhopCID\tdistance\tseqNum\n");
	 		printf("\t %d:\t%x\t%x\t%x\t%x\t%d\t%d\n",j+1,wbca->routeTable[j]->destnation,wbca->routeTable[j]->destCID,wbca->routeTable[j]->NextHop,wbca->routeTable[j]->nextCID,wbca->routeTable[j]->distance,wbca->routeTable[j]->seqNum);
	 	}
	 	printf("------------------END------------------\n");
		//WbcaOutputMNList(wbca);
	}

	
	int headerSize = sizeof(WbcaRTUPHeader);
	int RouteSize=sizeof(WbcaRTUP)*sizeRoute;
	int pktSize = headerSize+RouteSize; 
	Message *newMsg = NULL;
	char* pktPtr = NULL;

	UInt8 mesType = 7;
	NetworkRoutingProtocolType protocolType = ROUTING_PROTOCOL_WBCA;
	BOOL isDelay = FALSE;
	Address broadcastAddress;
	SetIPv4AddressInfo(&broadcastAddress, ANY_DEST);

	//printf("node %d boardcast RTUP message (with ip address %x)\n",node->nodeId,wbca->iface->address.interfaceAddr.ipv4);
	for(int i =0;i<node->numberInterfaces;i++)
	{

		newMsg = MESSAGE_Alloc(node,NETWORK_LAYER, protocolType, MSG_MAC_FromNetwork);
		MESSAGE_PacketAlloc(node, newMsg,pktSize, TRACE_WBCA);
		pktPtr = MESSAGE_ReturnPacket(newMsg);

		WbcaRTUPHeader *pointerheader = (WbcaRTUPHeader *)pktPtr;
		pointerheader->mesType = WBCA_RTUP;
		pointerheader->size =sizeRoute;
		pointerheader->destination.address = wbca->iface[i].address.interfaceAddr.ipv4;
		pointerheader->sourceAddr = ANY_IP;
		
		WbcaRTUP *pointerRTUP = (WbcaRTUP *)(pktPtr+sizeof(WbcaRTUPHeader));
		memcpy(pointerRTUP,(char*)RTUP,RouteSize);
		WbcaSendPacket(node, MESSAGE_Duplicate(node, newMsg), wbca->iface[i].address, *destAddr,i, 1, ANY_DEST,0,isDelay);

		MESSAGE_Free(node, newMsg);
	}
	MEM_free(RTUP);
	wbca->CR=0;
}

//接受上一层传过来的消息并处理 
void WbcaRouterFunction(Node* node,Message* msg,Address destAddr,Address previousHopAddress,BOOL* packetWasRouted)
{
    WbcaData* wbca=NULL;
	IpHeaderType* ipHeader=NULL;
	Address sourceAddress;
	wbca=(WbcaData *)NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_WBCA, NETWORK_IPV4);
	ipHeader =(IpHeaderType*)MESSAGE_ReturnPacket(msg);

	SetIPv4AddressInfo(&sourceAddress, ipHeader->ip_src);
    //Ignore WBCA packet and my own packet 忽略掉WBCA协议自身的消息包
	if(ipHeader && ipHeader->ip_p==IPPROTO_WBCA){

		//MESSAGE_Free(node, msg);
		return;
	}

	//只要不是WBCA本身的包 并且目的IP不是自身 则 *packetWasRouted=TRUE
	if(WbcaIPIsMyIp(node, destAddr)){
		*packetWasRouted=FALSE;
	}else{
		*packetWasRouted=TRUE;
	}

	if(!WbcaIPIsMyIp(node, sourceAddress)){ //源节点不是自己的话,WbcaHandleData
		WbcaHandleData(node,msg,destAddr);//éžå¼€å§‹èŠ‚ç‚¹
	}else{ //消息源节点是自己
		if(!(*packetWasRouted)){ //wbcaIpIsMyIp(node,destAddr)ï¼Œä¸ºç›®çš„èŠ‚ç‚¹
			return;
		}else{
			WbcaHandleData(node, msg, destAddr);
		}
	}

}

//-------------------------------------------
//-------------------------------------------
//-------------------------------------------



//The last 8bit of IP + 8bit 0 
UInt16
GetHeaderCID(WbcaData* wbca)
{
	UInt16 CID = 0;
	NodeAddress IP = wbca->iface->address.interfaceAddr.ipv4;
	CID = (IP - 0xc0000000) * 256;
	//printf("0x%x Get Header ID: 0x%x\n",IP,CID);

	return CID;
}






void
WbcaSendMes(Node* node,WbcaData* wbca, int mesType, UInt16 destCID, UInt32 destAddr,UInt16 info)
{	
	Message* newMsg = NULL;
	WbcaMesPacket* mesPkt = NULL;
	NetworkRoutingProtocolType protocolType = ROUTING_PROTOCOL_WBCA;
	char* pktPtr = NULL;
	int pktSize = sizeof(WbcaMesPacket);
	int i= 0;
	BOOL isDelay = false;

	newMsg = MESSAGE_Alloc(
				 node,
				 NETWORK_LAYER,
				 protocolType,
				 MSG_MAC_FromNetwork);
  
	MESSAGE_PacketAlloc(
		node,
		newMsg,
		pktSize,
		TRACE_WBCA);

	pktPtr = (char *) MESSAGE_ReturnPacket(newMsg);
	
	memset(pktPtr, 0, pktSize);
	
	// Section 8.4 of draft-ietf-manet-aodv-08.txt
	// Allocate the message and then broadcast to all interfaces
	// Hello Mesage 
	mesPkt = (WbcaMesPacket *) pktPtr;
	mesPkt->mesType = mesType;
	
	if(mesType == WBCA_JOIN)
	{
		mesPkt->info = 0;
	}
	else if(mesType == WBCA_OFFER)
	{
		mesPkt->info = info;
	}
	else if(mesType == WBCA_REQUEST)
	{
		mesPkt->info = info;
	}
	else if(mesType == WBCA_ACK)
	{
		mesPkt->info = info;
	}
	
	mesPkt->sourceAddr = wbca->iface[i].address.interfaceAddr.ipv4;
	mesPkt->destination.seqNum = wbca->seqNumber;
	mesPkt->destCID = destCID;
	mesPkt->destAddr = destAddr;
		
	for (i = 0; i < node->numberInterfaces; i++)
	{
		//ä»¿ç…§WBCA
		mesPkt = (WbcaMesPacket *) MESSAGE_ReturnPacket(newMsg);
		mesPkt->destination.address =
			wbca->iface[i].address.interfaceAddr.ipv4;
		
			Address destinationAddress;
            destinationAddress.networkType = NETWORK_IPV4;
            destinationAddress.interfaceAddr.ipv4 = destAddr;
			
		WbcaSendPacket(node, 
			MESSAGE_Duplicate(node, newMsg),
			wbca->iface[i].address, 
			destinationAddress, 
			i, 
			1, 
			ANY_DEST, 
			0,
			isDelay);

	}

	MESSAGE_Free(node, newMsg);

}

void getIconPath(unsigned id, int type, char path_2d[], char path_3d[]){
	int TOTALICONNUM = 10;
	char ret[25];
	char iconid[5];
	itoa(id%TOTALICONNUM,iconid,10); 
	char end_2d[] = ".png";
	char end_3d[] = ".3ds";
	
	if(type == 0){ //member
		strcpy(path_2d,"gui//icons//member");
		strcpy(path_3d,"gui//icons//member");	
	}else{  //leader 
		strcpy(path_2d,"gui//icons//leader");	
		strcpy(path_3d,"gui//icons//leader");	
	}
	
	strcat(path_2d,iconid);	
	strcat(path_2d,end_2d);
	strcat(path_3d,iconid);	
	strcat(path_3d,end_3d);
	return;
}

//å¤„ç†æ•°æ®åŒ?
void
WbcaHandleProtocolPacket(
    Node* node,
    Message* msg,
    Address srcAddr,
    Address destAddr,
    int ttl,
    int interfaceIndex)
{
	


    UInt8* packetType = (UInt8* )MESSAGE_ReturnPacket(msg);

	//trace recd pkt
	ActionData acnData;
	acnData.actionType = RECV;
	acnData.actionComment = NO_COMMENT;
	TRACE_PrintTrace(node, msg, TRACE_NETWORK_LAYER,
	PACKET_IN, &acnData , srcAddr.networkType);

	//MESSAGE_Free(node, msg);
	//return;

    switch (*packetType)
    {
        case Wbca_Hello:
        {	
        	// printf("\n\n node: %d receive Hello ",node->nodeId);
        	WbcaHelloPacket* pkt = (WbcaHelloPacket*) MESSAGE_ReturnPacket(msg);
			Address sourceAddress;
			Address destinationAddress;
			UInt32 dseqNum;
			UInt32 sseqNum;
		
			WbcaData* wbca = (WbcaData *) NetworkIpGetRoutingProtocol(node,
	                                                ROUTING_PROTOCOL_WBCA,
	                                                NETWORK_IPV4);
			UInt8 mesType = pkt->mesType;
			
	        SetIPv4AddressInfo(&sourceAddress,
	                            pkt->source.address);
	        SetIPv4AddressInfo(&destinationAddress,
	                             pkt->destination.address);
	        dseqNum = pkt->destination.seqNum;
	        sseqNum = pkt->source.seqNum;

			// get info
			Int16 CID = pkt->CID;
			if(DEBUG_MODE&0x01){
				printf("LOG:0x%x RECEIVE WBCA HELLO PACKET FROM",wbca->iface->address.interfaceAddr);
				printf(" 0x%x\n",srcAddr.interfaceAddr.ipv4);
			}
			
			Int8 conn = pkt->conn;
			double wei = pkt->Wei;
			Int8 Mn = pkt->Mn;
			Int8 state = pkt->state;
            double x=pkt->x;
            double y=pkt->y;
            double z=pkt->z;
            double m=CalMov(wbca,x,y,z);//m=calmov(wbca,x,y,z)
			WbcaUpdateMNList(wbca, node, srcAddr.interfaceAddr.ipv4, CID, conn,m, wei);

			bool isUn=0;
			bool isHeader=0;
			bool isMem=0;

			Int8 getMemId = CID % 256;
			Int8 getHeaderId = CID / 256;

			//printf("\n0x%x 0x%x\n",getHeaderId,getMemId);

			//where does the Hello packet come from
			if(getMemId == 0 && getHeaderId == 0)
			{
				isUn = 1;
			}
			else if(getMemId == 0 && getHeaderId != 0)
			{
				isHeader = 1;
			}
			else
			{
				isMem = 1;
			}

			if(isUn && wbca->state == 2 && state != 3)//Hello packet comes from a unclustering node
			{
				//compare weight
				if(wei < wbca->wei)
				{
					//printf("----------------------------------------------------");
					//printf("0x%x state change from 2 to 3 wei:%f  0x%x:%f\n",wbca->iface->address.interfaceAddr.ipv4,wbca->wei,srcAddr.interfaceAddr.ipv4,wei);
					//È¨ÖØ½ÏÐ¡£¬×ª»»Îª×¼´Ø³ÉÔ±×´Ì¬
					wbca->state = 3;insertTraceState(node,wbca->state);
					wbca->CID = 0;
					wbca->cnt = 0;
					
				}
			}
			else if(isHeader)//Hello Packet comes from a clusterheader
			{
				if(wbca->state != 4 && wbca->state != 5 && wbca->state != 6) //是未成簇状态
				{
					if(Mn < WBCA_MaxNumOfMem)
					{
						//send join packet
						wbca->Mn = 0;
						if(DEBUG_MODE&0x02){
							printf(" node: %d send WBCA_JOIN to ip %x \n",node->nodeId,srcAddr.interfaceAddr.ipv4);
						}
						WbcaSendMes(node, wbca, WBCA_JOIN, CID, srcAddr.interfaceAddr.ipv4, 0);
					}
				}
				else if(wbca->state == 5 && Mn < WBCA_MaxNumOfMem)  //是故吏卒头
				{
					//send join packet
					wbca->Mn = 0;
					if(DEBUG_MODE&0x02){
						printf(" node: %d send WBCA_JOIN to ip %x\n",node->nodeId,srcAddr.interfaceAddr.ipv4);
					}
					
					WbcaSendMes(node, wbca, WBCA_JOIN, CID, srcAddr.interfaceAddr.ipv4, 0);
				}
			}
			else if(isMem)//Hello Packet comes from a cluster member
			{
				//nothing to do				
			}   //update statics

		    MESSAGE_Free(node, msg);   
          
			break;
		}
		//ä¸‰æ¬¡æ¡æ‰‹
		case WBCA_JOIN:
		{
			if(DEBUG_MODE&0x02){
				printf("node: %d receive WBCA_JOIN from ip %x\n",node->nodeId,srcAddr.interfaceAddr.ipv4);
			}
			WbcaMesPacket* pkt = (WbcaMesPacket*) MESSAGE_ReturnPacket(msg);
			WbcaData* wbca = (WbcaData *) NetworkIpGetRoutingProtocol(node,
                                        ROUTING_PROTOCOL_WBCA,
                                        NETWORK_IPV4);

			if(wbca->state != 4 && wbca->state != 5)
			{
				MESSAGE_Free(node, msg);  
				break;
			}
			else if(pkt->destCID != wbca->CID)
			{	
				MESSAGE_Free(node, msg);  
				break;
			}
			else
			{
				while( !WbcaCheckMemID(wbca, wbca->memIdSeed) )
				{
					if(wbca->memIdSeed == 255)
					{
						wbca->memIdSeed = 1;
					}
					else
					{
						wbca->memIdSeed++;
					}
				}

				UInt16 memID = wbca->CID + wbca->memIdSeed;
				wbca->memIdSeed++;
				if(DEBUG_MODE&0x02){
					printf(" node: %d send WBCA_OFFER to ip %x \n",node->nodeId,srcAddr.interfaceAddr.ipv4);
				}

				WbcaSendMes(node, wbca, WBCA_OFFER, 0, srcAddr.interfaceAddr.ipv4, memID);
			}
			
			MESSAGE_Free(node, msg);
			break;
		}

		case WBCA_OFFER:
		{
			if(DEBUG_MODE&0x02){
				printf("node: %d receive WBCA_OFFER from ip %x\n",node->nodeId,srcAddr.interfaceAddr.ipv4);
			}
			
			WbcaMesPacket* pkt = (WbcaMesPacket*) MESSAGE_ReturnPacket(msg);
			WbcaData* wbca = (WbcaData *) NetworkIpGetRoutingProtocol(node,
                                        ROUTING_PROTOCOL_WBCA,
                                        NETWORK_IPV4);

			if(wbca->state == 4 || wbca->state == 6)
			{
				MESSAGE_Free(node, msg);  
				break;
			}						
			else if(pkt->destAddr != wbca->iface->address.interfaceAddr.ipv4)
			{
				MESSAGE_Free(node, msg);  
				break;
			}
			else
			{
				wbca->unacceptedCID = pkt->info;
				if(DEBUG_MODE&0x02){
					printf(" node: %d send WBCA_REQUEST to ip %x \n",node->nodeId,srcAddr.interfaceAddr.ipv4);
				}
				
				WbcaSendMes(node, wbca, WBCA_REQUEST, 0, srcAddr.interfaceAddr.ipv4, pkt->info);
			}
			
			MESSAGE_Free(node, msg);
			break;
		}

		case WBCA_REQUEST:
		{
			if(DEBUG_MODE&0x02){
				printf(" node: %d receive WBCA_REQUEST from ip %x \n",node->nodeId,srcAddr.interfaceAddr.ipv4);
			}
			
			WbcaMesPacket* pkt = (WbcaMesPacket*) MESSAGE_ReturnPacket(msg);
			WbcaData* wbca = (WbcaData *) NetworkIpGetRoutingProtocol(node,
                                        ROUTING_PROTOCOL_WBCA,
                                        NETWORK_IPV4);
			
			if(wbca->state != 4 && wbca->state != 5)
			{
				MESSAGE_Free(node, msg);  
				break;
			}
			else if(pkt->destAddr != wbca->iface->address.interfaceAddr.ipv4)
			{
				MESSAGE_Free(node, msg);  
				break;
			}
			else
			{
				WbcaInsertMemMNList(wbca, node, srcAddr.interfaceAddr.ipv4, pkt->info,wbca->wei);
					//	printf("----------------------------------------------------");
					//	printf("state change from %d to 4:CID=%Ox%x\n",wbca->state,wbca->CID);

		        wbca->state = 4;
				node->is_leader = 1;
				insertTraceState(node,wbca->state);
				clocktype currtime=getSimTime(node);
				
				char path_2d[35];
				char path_3d[35];
				getIconPath(wbca->iface->address.interfaceAddr.ipv4,1,path_2d,path_3d);
				//printf("LEADER_PATH:%s \n",path_3d);
				//--------delete----
				GUI_SetNodeIcon(node->nodeId,
								path_2d,
								currtime);
				GUI_SetNodeIcon(node->nodeId,
								path_3d,
								currtime);
				//--------delete----


				if(DEBUG_MODE&0x02){
					printf(" node: %d send WBCA_ACK to ip %x \n",node->nodeId,srcAddr.interfaceAddr.ipv4);
				}
				WbcaSendMes(node, wbca, WBCA_ACK, 0, srcAddr.interfaceAddr.ipv4, pkt->info);
			}

			MESSAGE_Free(node, msg);
			break;
		}

		case WBCA_ACK:
		{
			if(DEBUG_MODE&0x02){
				printf("node: %d receive WBCA_ACK from ip %x\n",node->nodeId,srcAddr.interfaceAddr.ipv4);
			}
			WbcaMesPacket* pkt = (WbcaMesPacket*) MESSAGE_ReturnPacket(msg);
			WbcaData* wbca = (WbcaData *) NetworkIpGetRoutingProtocol(node,
                                        ROUTING_PROTOCOL_WBCA,
                                        NETWORK_IPV4);
			if(wbca->state == 4 || wbca->state == 6)
			{
				MESSAGE_Free(node, msg);  
				break;
			}
			else if(pkt->destAddr != wbca->iface->address.interfaceAddr.ipv4)
			{
				MESSAGE_Free(node, msg);  
				break;
			}
			else if(wbca->state != 4 && wbca->state != 6)
			{
					//	printf("----------------------------------------------------");
					//	printf("0x%x state change from %d to 6 CID:0x%x\n",wbca->iface->address.interfaceAddr.ipv4,wbca->state,wbca->unacceptedCID);
					//  ³ÉÎª´Ø½Úµã

				wbca->state = 6;clocktype currtime=getSimTime(node);
				insertTraceState(node,wbca->state);
					
				char path_2d[35];
				char path_3d[35];
				getIconPath(srcAddr.interfaceAddr.ipv4,0,path_2d,path_3d);
				
				//--
				GUI_SetNodeIcon(node->nodeId,
								path_2d,
								currtime);
				
				GUI_SetNodeIcon(node->nodeId,
								path_3d,
								currtime);
		            
				wbca->CID = wbca->unacceptedCID;
				wbca->Mn = 1;
			}

			MESSAGE_Free(node, msg);
			break;
		}
		case WBCA_RTUP:{
			char* pktPtr = MESSAGE_ReturnPacket(msg);
			// break;
			WbcaData* wbca = (WbcaData *)NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_WBCA, NETWORK_IPV4);
			WbcaRTUPHeader *header = (WbcaRTUPHeader *)pktPtr;
			WbcaRTUP *RTUP = (WbcaRTUP *)(pktPtr+sizeof(WbcaRTUPHeader));
			int numroute = header->size;
			if(wbca->state == 4||wbca->state ==5){
				printf("--leader node %d receive the RTUP message from %x--\n",node->nodeId,srcAddr.interfaceAddr.ipv4);
				printf("---------DISPLAY received RTUP message ---------\n");
 	            for(int i =0;i<numroute;i++){
 	            	printf("\t NO\tdestIP\tdestCID\tdistance\tseqNum\n");
 	            	printf("\t %d\t%x\t%x\t%d\t%d\n",i+1,RTUP[i].destIP,RTUP[i].destCID,RTUP[i].distance,RTUP[i].seqNum);	
 	            }
			 	printf("-----------------------END----------------------\n");
				NodeAddress ssAddress = RTUP[0].destIP;
				Int16 ssCID = RTUP[0].destCID;
	            for(int i =0;i<numroute;i++){
					WbcaInsertReplaceRouteTable(node, wbca, RTUP[i].destIP,RTUP[i].destCID ,ssAddress, ssCID,RTUP[i].distance,RTUP[i].seqNum);
	            }
			}
			MESSAGE_Free(node, msg);
			break;
		}
        default:
        {
            //ERROR_Assert(FALSE, "Unknown packet type for Wbca!!!!");
            MESSAGE_Free(node, msg);
            break;
        }
		
    }
	

}


void
WbcaHandleProtocolEvent(
	Node* node,
	Message* msg)
{
	WbcaData* wbca = NULL;

	wbca = (WbcaData *) NetworkIpGetRoutingProtocol(
							node,
							ROUTING_PROTOCOL_WBCA,
							NETWORK_IPV4);
	
	switch (MESSAGE_GetEvent(msg))
	{
		
		case MSG_WBCA_SendHello:
		{
			//printf("LOG:WBCA SEND HELLO EVENT %d",node->nodeId);
		    //printf("\n");
			Address* destAddr = (Address* ) MESSAGE_ReturnInfo(msg);

			//counter changes
			if(wbca->state == 1 && wbca->cnt != 6)
			{
				wbca->cnt++;
			}
			else if(wbca->state == 1 && wbca->cnt == 6)
			{			
				if(wbca->numOfMN == 0)
				{   
					clocktype currtime=getSimTime(node);
					printf("node %d become lonely leader\n",node->nodeId);
					wbca->state = 5;
					node->is_leader = 1;
				    insertTraceState(node,wbca->state);
					wbca->CID = GetHeaderCID(wbca);
					//printf("0x%x state change from 1 to 5 time=%s\n",wbca->CID,timeString);
					wbca->conn = 0;
					GUI_SetNodeIcon(node->nodeId,
							"gui/icons/tank.png",
							currtime);
					
				}
				else
				{
					//printf("----------------------------------------------------");
					//printf("0x%x state change from 1 to 2 wei:%f\n",wbca->iface->address.interfaceAddr.ipv4,wbca->wei);
					wbca->state = 2;
					insertTraceState(node,wbca->state);
					
					wbca->CID = 0;
				}
				wbca->cnt = 0;
			}
			else if(wbca->state == 2 && wbca->cnt !=3)
			{
				wbca->cnt++;
			}
			else if(wbca->state == 2 && wbca->cnt ==3)
			{  
				printf("node %d become lonely leader\n",node->nodeId);
				wbca->state = 5;
				node->is_leader =1;
				insertTraceState(node,wbca->state);
				wbca->CID = GetHeaderCID(wbca);
				//printf("0x%x state change from 2 to 5 time=%s\n",wbca->CID,timeString);
				wbca->cnt = 0;
				
				// ±ä³É¹ÂÁ¢´ØÍ·
				clocktype currtime=getSimTime(node);
				GUI_SetNodeIcon(node->nodeId,
							"gui/icons/tank.png",
							currtime);
			}
			else if(wbca->state == 3 && wbca->cnt !=6)
			{
				wbca->cnt++;
			}
			else if(wbca->state == 3 && wbca->cnt ==6)
			{ 
				printf("node %d become lonely leader\n",node->nodeId);
				wbca->state = 5;
				node->is_leader=1;
				insertTraceState(node,wbca->state);
				wbca->CID = GetHeaderCID(wbca);
				wbca->cnt = 0;
				
				//printf("0x%x state change from 3 to 5 time=%s\n",wbca->CID,timeString);
				clocktype currtime=getSimTime(node);
	            GUI_SetNodeIcon(node->nodeId,
							"gui/icons/tank.png",
							currtime);
			}
			
			if(wbca->state != 3 && wbca->state != 2)
			{
			wbca->wei =CalWei(wbca);
			}

			//MNLIST 这里要找出那些已经没有成员的簇头
			WbcaCheckMNList(wbca, node);
			wbca->numOfMem = NumOfMem(wbca);
			wbca->conn = wbca->numOfMN;

			//å­¤ç«‹ç°‡å¤´å¤„ç†
			if(wbca->state == 4 && NumOfMem(wbca) == 0)
			{	
				//printf("----------------------------------------------------");
				//printf("0x%x state change from 4 to 5 wei:%f\n",wbca->iface->address.interfaceAddr.ipv4,wbca->wei);
				printf("node %d become lonely leader\n",node->nodeId);
				wbca->state = 5;
				node->is_leader=1;
				clocktype currtime=getSimTime(node);
				insertTraceState(node,wbca->state);
					//--
		            GUI_SetNodeIcon(node->nodeId,
							"gui/icons/tank.png",
							currtime);
			}

			//那些孤立簇头有了成员
			if(wbca->state == 5 && NumOfMem(wbca) != 0)
			{
				

				wbca->state = 4;insertTraceState(node,wbca->state);
				wbca->checkMemNum = 0;
				clocktype currtime=getSimTime(node);

		        char path_2d[35];
				char path_3d[35];
				getIconPath(wbca->iface->address.interfaceAddr.ipv4,1,path_2d,path_3d);
			
				//--
				GUI_SetNodeIcon(node->nodeId,
								path_2d,
								currtime);
				GUI_SetNodeIcon(node->nodeId,
								path_3d,
								currtime);
				
			}
			if(DEBUG_MODE&0x01){
				printf("\n\n node: %d boardcasting Hello ",node->nodeId);
			}
			
			WbcaBroadcastHelloMessage(node, wbca, destAddr);

			//printf("wbca->state %d getSimTime(node) - getSimStartTime(node) %u \n",wbca->state,getSimTime(node) - getSimStartTime(node));
			//&&(getSimTime(node) - getSimStartTime(node) > (10 * SECOND))
			if((wbca->state == 4||wbca->state== 5))
			{
				WbcaInsertReplaceRouteTable(node,wbca,wbca->iface->address.interfaceAddr.ipv4 , wbca->CID, wbca->iface->address.interfaceAddr.ipv4,wbca->CID, 0, 0);
	            for(int m=0;m<wbca->numOfMN;m++){ //自己是簇头,然后检查邻居表,把邻居表里面是簇头的加到自己的路由表中
	            	if(wbca->mnlist[m]->okHeader){
	            		WbcaInsertReplaceRouteTable(node, wbca, wbca->mnlist[m]->IP,  wbca->mnlist[m]->CID,wbca->mnlist[m]->IP, wbca->mnlist[m]->CID, 0, 0);
	            	}
	            }
			}

			if((wbca->state==4||wbca->state==5)&& wbca->CR>0 ) //是簇头并且路由表有过修改
			{
				// printf("WbcaRTUPMessage \n");	
				WbcaRTUPMessage( node, wbca, destAddr);
			}

			clocktype delay =
				(clocktype) WBCA_PC_ERAND(wbca->wbcaJitterSeed);
			MESSAGE_Send(node, msg, WBCA_HELLO_INTERVAL + delay);


			//ä¸è¶…è¿?0ä¸ªç°‡æˆå‘˜çš„ç°‡ä¼šè§£æ•?
			if(wbca->checkMemNum != 3 && wbca->state == 4)
			{
				wbca->checkMemNum++;
			}
			else if(wbca->checkMemNum == 3 && wbca->state == 4)
			{
				wbca->checkMemNum = 0;
				if(wbca->numOfMem < 10&&wbca->numOfMN>10)
				{
					wbca->state = 2;insertTraceState(node,wbca->state);
					clocktype currtime=getSimTime(node);
					//--
		            GUI_SetNodeIcon(node->nodeId,
							"gui/icons/default.png",
							currtime);
				
					wbca->CID = 0;
					wbca->numOfMem = 0;
					wbca->cnt = 0;
				}
			}

			break;
		}
		
		default:
		{
			ERROR_Assert(FALSE, "Wbca: Unknown MSG type!\n");
			break;
		}
		
		
	}

	//if(wbca->state!=1&&wbca->state!=2&&wbca->state!=3&&nodeState[node->nodeId]!=1)
	{
	
	//ofstream out("out.txt");
	//printf("\n--------------------------------------------------\n");
	
	//printf("Node Id\t\t\t\t= %u\n",node->nodeId);
	{
	//ofstream out("cpp_home.txt");
	//out<<node->nodeId;
	//out.close();
	//char buf[MAX_STRING_LENGTH];
	
	//TIME_PrintClockInSecond(getSimTime(node), buf);
	//printf("Current Sim Time[s]\t\t= %s\n", buf);
	
	//printf("\n--------------------------------------------------\n");
	
	}
	//nodeState[node->nodeId]=1;
	
	}

}

//-----------------------------------------------------------------------------
// FUNCTIONS WITH EXTERNAL LINKAGE
//-----------------------------------------------------------------------------
// /**
// FUNCTION   :: RoutingWBCAInitTrace
// LAYER      :: APPLCATION
// PURPOSE    :: WBCA initialization  for tracing
// PARAMETERS ::
// + node : Node* : Pointer to node
// + nodeInput    : const NodeInput* : Pointer to NodeInput
// RETURN ::  void : NULL
// **/

void RoutinWBCAInitTrace(Node* node, const NodeInput* nodeInput)
{
    char buf[MAX_STRING_LENGTH];
    BOOL retVal;
    BOOL traceAll = TRACE_IsTraceAll(node);
    BOOL trace = FALSE;
    static BOOL writeMap = TRUE;

    IO_ReadString(
        node->nodeId,
        ANY_ADDRESS,
        nodeInput,
        "TRACE-WBCA",
        &retVal,
        buf);
	
    if (retVal)
    {
        if (strcmp(buf, "YES") == 0)
        {
            trace = TRUE;
        }
        else if (strcmp(buf, "NO") == 0)
        {
            trace = FALSE;
        }
        else
        {
            ERROR_ReportError(
                "TRACE-WBCA should be either \"YES\" or \"NO\".\n");
        }
    }
    else
    {
        if (traceAll)
        {
            trace = TRUE;
        }
    }

    if (trace)
    {
            TRACE_EnableTraceXML(node, TRACE_WBCA,
                "WBCA", RoutingWBCAPrintTraceXML, writeMap);
    }
    else
    {
            TRACE_DisableTraceXML(node, TRACE_WBCA,
                "WBCA", writeMap);
    }
    writeMap = FALSE;
}
//-----------------------------------------------------------------------------
// Print TraceXML function
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// FUNCTION   :: RoutingWBCAPrintTraceXML
// LAYER      :: NETWORK
// PURPOSE    :: Print packet trace information in XML format
// PARAMETERS ::
// + node     : Node*    : Pointer to node
// + msg      : Message* : Pointer to packet to print headers from
// RETURN     ::  void   : NULL
//-----------------------------------------------------------------------------

void RoutingWBCAPrintTraceXML(Node* node, Message* msg)
{
    char buf[MAX_STRING_LENGTH];
    char sourceAddr[MAX_STRING_LENGTH];
    char destinationAddr[MAX_STRING_LENGTH];
    char nexthopAddr[MAX_STRING_LENGTH];
     
    WBCA *wbca = (WBCA *) node->appData.wbca;
    RoutingWBCAHeader *header;
    AdvertisedRoute *payload;
    int numAdvertisedRoutes;
	

    // If WBCA is not configured here, then discard message.
    if (!wbca)
    {
        return;
    }
    // Obtain pointers to header, payload.

	//WbcaData* wbca1 = (WbcaData *) NetworkIpGetRoutingProtocol(node,ROUTING_PROTOCOL_WBCA,NETWORK_IPV4);
	
  //node->nodeState=wbca1->state;
    header = (RoutingWBCAHeader *) msg->packet;
    payload = (AdvertisedRoute *)
              (msg->packet + sizeof(RoutingWBCAHeader));

    IO_ConvertIpAddressToString(header->sourceAddress, sourceAddr);
    IO_ConvertIpAddressToString(header->destAddress , destinationAddr);

     // Obtain number of rows in update.

    sprintf(buf, "<WBCA>");
    TRACE_WriteToBufferXML(node, buf);

    sprintf(buf,"%s %s %d",
             sourceAddr,
             destinationAddr,
             header->payloadSize );
    TRACE_WriteToBufferXML(node, buf);

    numAdvertisedRoutes = header->payloadSize / sizeof(AdvertisedRoute);

    for(int i = 0; i < numAdvertisedRoutes; i++) {

    IO_ConvertIpAddressToString(payload->destAddress , destinationAddr);
    IO_ConvertIpAddressToString(payload->subnetMask, sourceAddr);
    IO_ConvertIpAddressToString(payload->nextHop, nexthopAddr);

    sprintf(buf,"<advertisedRoute>%s %s %s %d </advertisedRoute>",
                    destinationAddr,
                    sourceAddr,
                    nexthopAddr,
                    payload->distance     
	);
    TRACE_WriteToBufferXML(node, buf);
    payload++;
    }
	

    sprintf(buf, "</WBCA>");
    TRACE_WriteToBufferXML(node, buf);
}


//接受上一层传过来的消息并处理  
void Wbca4RouterFunction(
	           Node* node,
	           Message* msg, 
	           NodeAddress destAddr, 
	           NodeAddress previousHopAddress,
	           BOOL* packetWasRouted)
{
	Address destAddress;
	Address previousHopAddr;
	destAddress.networkType=NETWORK_IPV4;
	destAddress.interfaceAddr.ipv4=destAddr;
	previousHopAddr.interfaceAddr.ipv4=previousHopAddress;
	if(previousHopAddress){
		previousHopAddr.networkType=NETWORK_IPV4;
	}else{
		previousHopAddr.networkType=NETWORK_INVALID;
	}
	char * pktPtr = (char *) MESSAGE_ReturnPacket(msg);
	WbcaMesPacket* mesPkt = (WbcaMesPacket *) pktPtr;
	WbcaHelloPacket* helpkt = (WbcaHelloPacket*)pktPtr;

	WbcaRouterFunction(node,msg,destAddress,previousHopAddr,packetWasRouted);
}