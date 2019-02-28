#define Wbca_Hello 1
#define Wbca_Mes   2
#define Wbca_Get_HeaderID 65280//0x1111111100000000 = 65280
#define Wbca_Get_MemID 255//0x0000000011111111 = 255
#define Wbca_Conn_To1 200

#define WBCA_BROADCAST_JITTER          (20 * MILLI_SECOND)
#define WBCA_HELLO_INTERVAL	 (0.5 * SECOND)
#define WBCA_THREE_INTERVAL	 (0.5 * SECOND)

#define WBCA_MNLIST_SIZE  200
#define WBCA_MaxNumOfMem  20
#define WBCA_ROUTE_HASH_TABLE_SIZE 200
#define WBCA_Max 1000

#define WBCA_JOIN 3
#define WBCA_OFFER 4
#define WBCA_REQUEST 5
#define WBCA_ACK 6
#define WBCA_RTUP 7


#define COLLECTING    1
#define COMPARING     2
#define PRE_MEMBER    3
#define LEADER        4
#define LONELY_LEADER 5
#define MEMBER        6


typedef struct str_wbca_interface_info
{
    Address address;
    UInt32 ip_version;
    BOOL wbca4eligible;
    BOOL wbca6eligible;
    BOOL AFlag;
} WbcaInterfaceInfo;

typedef struct
{
    NodeAddress address;
    UInt32 seqNum;
} WbcaAddrSeqInfo;

typedef struct struct_wbca_MNList
{
	NodeAddress IP;
	Int16 CID;
	long rssi;	//用于记录节点间的距离。每接收一次Hello消息，则会更新此项。
	long rssi_d;//连续Hello消息间RSSi的差值。初始默认为1。
	Int16 conn;  //节点的邻居节点个数
	double wei;  //节点的权值。  
	Int8 CM;  //标记节点的状态
	clocktype TTL; //用于记录接收到消息的当前系统时间，每收到Hello消息，则会更新此项。
	double x,y,z;
	double m;
    BOOL okHeader;
	//MobilityData* mobilityData;
	//int nodeId;
}WbcaMNList;


typedef struct struct_wbca_RouteTable
{
 NodeAddress destnation;
 Int16 destCID;
 NodeAddress NextHop;
 Int16 nextCID;
 Int32 distance;
 UInt32 seqNum;
} WbcaRouteTable;

typedef struct struct_network_wbca_str
{
	UInt16 CID;
	UInt16 unacceptedCID;//未成簇节点收到offer后，用此变量保存CID
	Int16 conn;  //节点的连接度即节点邻居节点个数
	double mob;  //节点的移动度
	double wei;  //节点的权值
	Int8 state;  //节点的状态
    Int8 Ms;     //标志位。0：节点不处于三次握手过程，1：节点处于三次握手过程
	Int8 cnt;    //计数器
	Int8 Mn;     //簇成员节点个数，未分簇节点和簇成员节点发送的Hello中Mn为0
    WbcaMNList* mnlist[WBCA_MNLIST_SIZE]; //邻居表

    WbcaRouteTable* routeTable[WBCA_ROUTE_HASH_TABLE_SIZE]; //路由表
    Int16 numOfRoute;//路由表内成员个数
    Int16 CR;        //簇头路由表变化

	Int8 checkMemNum;
	Int16 numOfMN; //邻居个数
	Int16 numOfMem;//麾下成员个数
	UInt8 memIdSeed;
	double M;
	double m;
    double x,y,z;
	//作为成员 3次收到来自簇头的hello 连续3次簇头的邻居表中没有自己时,自己变为Pre_member 
    unsigned short lost_leader_count; 
    MobilityData* mobilityData;
	int change;

    //same as Aodv
	WbcaInterfaceInfo* iface;//找IP
	UInt32 seqNumber;
	RandomSeed  wbcaJitterSeed;
	Address defaultInterfaceAddr;
	int defaultInterface;
    NodeAddress temporaryip;
	clocktype temporarytime;
	clocktype lastBroadcastSent;

    //------------------lkn
    BOOL statsCollected;
    BOOL statsPrinted;




}WbcaData;

typedef struct Wbca_Hello_Packet
{
	//72bit:8bit for type; 16bit for CID; 8bit for conn; 32bit for Wei; 8bit for Mn;

	UInt8 mesType;
	UInt16 CID;
	UInt8 conn;
	double Wei;
    
	Int8 Mn;
	Int8 state;
	double x,y,z;
	// destination address and sequence
    WbcaAddrSeqInfo destination;

    //簇头节点广播hello的时候,要携带自己的邻居表都有哪些节点
    NodeAddress my_neighbors[WBCA_MNLIST_SIZE];
    Int8 my_neighbor_number;

    // source address and sequence
    WbcaAddrSeqInfo source;
	NodeAddress sourceAddr;
}WbcaHelloPacket;

typedef struct Wbca_Mes_Packet
{
	UInt8 mesType;
	UInt8 len;
	UInt16 info;
	UInt16 destCID;
	UInt32 destAddr; 
  
	// destination address and sequence
    WbcaAddrSeqInfo destination;

    // source address and sequence
    WbcaAddrSeqInfo source;
	NodeAddress sourceAddr;
}WbcaMesPacket;

//RTUP消息的内容主体
typedef struct Wbca_RTUP
{
    NodeAddress destIP;
    Int16 destCID;
    Int16 distance;
    UInt32 seqNum;  
}WbcaRTUP;


//RTUP消息的头部
typedef struct Wbca_RTUP_Packet
{
    UInt8 mesType;
    WbcaAddrSeqInfo destination;
    WbcaAddrSeqInfo source;
    NodeAddress sourceAddr;
    int size;
}WbcaRTUPHeader;

//路由功能
void 
Wbca4RouterFunction(Node* node,Message* msg,NodeAddress destAddr,NodeAddress previousHopAddress,BOOL* packetWasRouted);


typedef struct
{
    NodeAddress destAddress;
    NodeAddress subnetMask;
    NodeAddress nextHop;
    int         incomingInterface;
    int         outgoingInterface;

    short       distance;
    BOOL        localRoute;

    clocktype   refreshTime;
    BOOL        changed;
    // This flag is set if its a re-distributed route
    BOOL        isPermanent;
} Route;

//
// Header fields for route-advertisement packet.
//

typedef struct
{
    NodeAddress sourceAddress;  // source IP address
    NodeAddress destAddress;    // destination IP address
    int         payloadSize;    // size of payload in bytes
} RoutingWBCAHeader;
//
// Route in a route-advertisement packet.
//

typedef struct
{
    NodeAddress destAddress;
    NodeAddress subnetMask;
    NodeAddress nextHop;

    int         distance;
} AdvertisedRoute;

// /**
// STRUCT         ::    AodvStats
// DESCRIPTION    ::    AODV IPv4/IPv6 Structure to store the statistical
//                      informations.
// **/
typedef struct
{
    D_UInt32 numRequestInitiated;
    UInt32 numRequestResent;
    UInt32 numRequestRelayed;
    UInt32 numRequestForLocalRepair;
    UInt32 numRequestForAlternateRt;
    UInt32 numRequestRecved;
    UInt32 numRequestDuplicate;
    UInt32 numRequestTtlExpired;
    UInt32 numSenderInBlacklist;
    UInt32 numRequestRecvedAsDest;
    UInt32 numReplyInitiatedAsDest;
    UInt32 numReplyInitiatedAsIntermediate;
    UInt32 numReplyForwarded;
    UInt32 numGratReplySent;
    UInt32 numReplyRecved;
    UInt32 numReplyRecvedForLocalRepair;
    UInt32 numReplyRecvedAsSource;
    UInt32 numHelloSent;
    UInt32 numHelloRecved;
    UInt32 numRerrInitiated;
    UInt32 numRerrInitiatedWithNFlag;
    UInt32 numRerrForwarded;
    UInt32 numRerrForwardedWithNFlag;
    UInt32 numRerrRecved;
    UInt32 numRerrRecvedWithNFlag;
    UInt32 numRerrDiscarded;
    UInt32 numDataInitiated;
    UInt32 numDataForwarded;
    UInt32 numDataRecved;
    UInt32 numDataDroppedForNoRoute;
    UInt32 numDataDroppedForOverlimit;
    UInt32 numMaxHopExceed;
    UInt32 numHops;
    UInt32 numRoutes;
    UInt32 numBrokenLinks;

    // Added to track table's max entries
    UInt32 numMaxSeenTable;

    // Added to track LastFound matches
    UInt32 numLastFoundHits;
} WBCAStats;

typedef struct
{
    // General state information.

    clocktype nextPeriodicUpdateTime;
    BOOL triggeredUpdateScheduled;

    // Route table.

    Route *routeTable;
    int numRoutesAllocated;
    int numRoutes;

    // Statistics.

    int numPeriodicUpdates;
    int numTriggeredUpdates;
    int numRouteTimeouts;
    int numRouteAdvertisementsReceived;
    RandomSeed periodicJitterSeed;
    RandomSeed checkTimeoutSeed;
    RandomSeed triggeredUpdateSeed;
    BOOL statsPrinted;
} WBCA;

void
RoutingWBCAfordFinalize(Node *node);

/*
// FUNCTION   :: RoutingWBCAInitTrace
// PURPOSE    :: WBCA initialization  for tracing
// PARAMETERS ::
// + node : Node* : Pointer to node
// + nodeInput    : const NodeInput* : Pointer to NodeInput
// RETURN ::  void : NULL
*/
void RoutingWBCAInitTrace(Node* node, const NodeInput* nodeInput);

//
// FUNCTION   :: RoutingWBCAPrintTraceXML
// PURPOSE    :: Print packet trace information in XML format
// PARAMETERS ::
// + node     : Node*    : Pointer to node
// + msg      : Message* : Pointer to packet to print headers from
// RETURN     ::  void   : NULL

void RoutingWBCAPrintTraceXML(Node * node,Message * msg);

void WbcaInit(
    Node *node,
    WbcaData **aodvPtr,
    const NodeInput *nodeInput,
    int interfaceIndex,
    NetworkRoutingProtocolType wbcaProtocolType);

void 
    WbcaFinalize(Node* node,int interfaceIndex);
//insert and update the route table
void 
WbcaInsertReplaceRouteTable(Node* node,
                                   WbcaData* wbca,
                                   NodeAddress destIp,
                                   NodeAddress nextIP,
                                   Int16 destCID,
                                   Int16 nextCID,
                                   Int16 distance,
                                   UInt32 seqNum);

void
WbcaHandleProtocolPacket(
    Node* node,
    Message* msg,
    Address srcAddr,
    Address destAddr,
    int ttl,
    int interfaceIndex);

void
WbcaHandleProtocolEvent(
    Node *node,
    Message *msg);



