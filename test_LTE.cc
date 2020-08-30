/*
 *
 * /ns-3.29/my-example.ccより
 * 
 * ofswitch13使用
 * 
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/ofswitch13-module.h"
#include "ns3/socket.h"
#include <string>

int i = 0;
int j = 0;

//#include "myCsmaOFC.h"

using namespace ns3;

class TakumaController;

void modify ()
{
    std::ostringstream oss;
    oss << "Update:" << Simulator::Now ().GetSeconds ();
    // Every update change the node description for node 2
    std::ostringstream node0Oss;
    node0Oss << "-----Node:" << Simulator::Now ().GetSeconds ();
    if (Simulator::Now ().GetSeconds () < 10) // This is important or the simulation
    {
        // will run endlessly
        Simulator::Schedule (Seconds (1), modify);
    }
}

void
SetPosition (Ptr<Node> node, Vector position)
{
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
    mobility->SetPosition (position);
}

Vector 
GetPosition (Ptr<Node> node) 
{ 
	Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> (); 
	return mobility->GetPosition (); 
} 

void
AdvancePosition (Ptr<Node> node)
{
    Vector pos = GetPosition (node);
    pos.x += 10.0;
    SetPosition (node, pos);
    Simulator::Schedule (Seconds (1.0), AdvancePosition, node);
}

NS_LOG_COMPONENT_DEFINE("Study");
    
int
main (int argc, char *argv[])
{
    bool verbose = false;
    bool trace = false;
    uint16_t numNodes = 7;
    uint16_t src_sink = 2;
    uint16_t sourceNode = 0;
    uint16_t sinkNode = numNodes - 1;
    uint16_t numSwitches = 3;
    Time simTime = Seconds (10.0);
    Time simStart1 = Seconds (1.0);
    Time simStop1 = Seconds (5.0);
    Time simStart2 = Seconds (5.0);
    Time simStop2 = Seconds (10.0);
    Time interPacketInterval = MilliSeconds (100);
    double distance = 200.0;    // m
    uint32_t packetSize = 1024; //byte

    // Configure command line parameters
    CommandLine cmd;
    cmd.AddValue ("verbose", "Enable verbose output", verbose);
    cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
    cmd.AddValue ("numNodes", "Number of nodes", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
    cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
    cmd.AddValue ("distance", "Distance between nodes", distance);
    cmd.AddValue ("packetSize", "Size of packet", packetSize);
    cmd.Parse (argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults ();

    // parse again so you can override default values from the command line
    cmd.Parse(argc, argv);

    if (verbose)
    {
        OFSwitch13Helper::EnableDatapathLogs ();
        LogComponentEnable ("OFSwitch13Interface", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Device", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Port", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Queue", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13SocketHandler", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13LearningController", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
    }

    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Enable checksum computations (required by OFSwitch13 module)
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

    /*
     *      LTE
     */
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<EpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    
    lteHelper->SetEpcHelper (epcHelper);

    // PGWノード生成
    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    // Create a single server
    NodeContainer serverContainer;
    serverContainer.Create (1);
    Ptr<Node> server = serverContainer.Get (0);
    InternetStackHelper internet;
    internet.Install (serverContainer);

    // Create the Internet
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2p.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));

    // PGW -- server
    NetDeviceContainer internetDevices = p2p.Install (pgw, server);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    // interface 0 is localhost, 1 is the p2p device
    // Ipv4Address serverAddr = internetIpIfaces.GetAddress (1);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> serverStaticRouting = ipv4RoutingHelper.GetStaticRouting (server->GetObject<Ipv4> ());
    serverStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);


    // Create the UE node
    NodeContainer ueNodes;
    ueNodes.Create (numNodes);

    // Create the eNB node
    NodeContainer enbNodes;
    enbNodes.Create (src_sink);

    // Create the switch node
    NodeContainer switches;
    switches.Create(numSwitches);


    /*
     *          OPENFLOW
     */
    // Use the CsmaHelper to connect host nodes to the switch node
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

    NetDeviceContainer ueDev;
    NetDeviceContainer switchPorts[numSwitches];
    for (uint16_t i = 0; i < numSwitches; i++)
    {
        NodeContainer pair (ueNodes.Get (i*3), switches.Get(i));
        NetDeviceContainer link = csma.Install (pair);
        ueDev.Add (link.Get (0));
        switchPorts[i].Add (link.Get (1));
    }

    NodeContainer pair1 (switches.Get (0), switches.Get(1));
    NetDeviceContainer link1 = csma.Install (pair1);
    switchPorts[0].Add (link1.Get (0));
    switchPorts[1].Add (link1.Get (1));
    NodeContainer pair2 (switches.Get (1), switches.Get(2));
    NetDeviceContainer link2 = csma.Install (pair2);
    switchPorts[1].Add (link2.Get (0));
    switchPorts[2].Add (link2.Get (1));
    NodeContainer pair3 (switches.Get (2), switches.Get(0));
    NetDeviceContainer link3 = csma.Install (pair3);
    switchPorts[2].Add (link3.Get (0));
    switchPorts[0].Add (link3.Get (1));


    // Create two controller nodes
    NodeContainer controllers;
    controllers.Create (1);


    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (int i = 0; i < numSwitches; i++)
    {
        /*
        if (i % 3 == 0){
            positionAlloc->Add(Vector(distance * (i / 3), 200, 0));
        }
        else if (i % 3 == 1){
            positionAlloc->Add(Vector(distance * ((i - 1) / 3) + distance / 2, 170, 0));
        }
        else{
            positionAlloc->Add(Vector(distance * ((i - 2) / 3) + distance / 2, 230, 0));
        }
        */
        positionAlloc->Add(Vector(distance * i, 200, 0));
    }
    for (int i = 0; i < numNodes; i++)
    {
        if (i % 3 == 0){
            positionAlloc->Add(Vector(distance * (i / 3), 210, 0));
        }
        else if (i % 3 == 1){
            positionAlloc->Add(Vector(distance * ((i - 1) / 3) + distance / 2, 180, 0));
        }
        else{
            positionAlloc->Add(Vector(distance * ((i - 2) / 3) + distance / 2, 240, 0));
        }
    }
    for (uint16_t i = 0; i < src_sink; i++)
    {
        positionAlloc->Add(Vector(distance * 2 * i + 50, 150, 0));
    }
    positionAlloc->Add(Vector(distance * 2, 50, 0));
    positionAlloc->Add(Vector(distance + 50, 120, 0));
    positionAlloc->Add(Vector(distance + 50, 80, 0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install (switches);
    mobility.Install (ueNodes);
    mobility.Install (enbNodes);
    mobility.Install (controllers);
    mobility.Install (pgw);
    mobility.Install (server);

    /*
     *          OPENFLOW
     */
    // Configure the OpenFlow network domain
    Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper> ();
    Ptr<TakumaController> ctrl = CreateObject<TakumaController> ();

    of13Helper->InstallController (controllers.Get (0), ctrl);
    for (int i = 0; i < numSwitches; i++)
    {
        of13Helper->InstallSwitch (switches.Get(i), switchPorts[i]);
    }
    of13Helper->CreateOpenFlowChannels ();


    /*
     *          LTE
     */
    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (NodeContainer(ueNodes.Get(sourceNode), ueNodes.Get(sinkNode)));

    // Install the IP stack on the UEs
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint16_t u = 0; u < src_sink; u++)
    {
        Ptr<Node> ueNode = ueNodes.Get (u*(numNodes-1));
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < src_sink; i++)
    {
        lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
        // side effect: the default EPS bearer will be activated
    }


    // Set IPv4 host addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer ueIpIface_csma;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    ueIpIface_csma = address.Assign (ueDev);


    // LTE
    UdpServerHelper Server2 (20);
    ApplicationContainer serverApps2 = Server2.Install(ueNodes.Get(sinkNode));
    serverApps2.Start (simStart1);
    serverApps2.Stop (simStop1);

    UdpClientHelper Client2(ueIpIface.GetAddress(sinkNode-5),20);
    Client2.SetAttribute ("MaxPackets", UintegerValue (10000));
    Client2.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client2.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps2 = Client2.Install(ueNodes.Get(sourceNode));
    clientApps2.Start (simStart1);
    clientApps2.Stop (simStop1);

    lteHelper->EnableTraces ();

    // csma
    UdpServerHelper Server1 (9);
    ApplicationContainer serverApps1 = Server1.Install(ueNodes.Get(sinkNode));
    serverApps1.Start (simStart2);
    serverApps1.Stop (simStop2);

    UdpClientHelper Client1(ueIpIface_csma.GetAddress(2),9);
    Client1.SetAttribute ("MaxPackets", UintegerValue (10000));
    Client1.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client1.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps1 = Client1.Install(ueNodes.Get(sourceNode));
    clientApps1.Start (simStart2);
    clientApps1.Stop (simStop2);


    // Enable datapath stats and pcap traces at ueNodes, switch(es), and controller(s)
    if (trace)
    {
        of13Helper->EnableOpenFlowPcap ("openflow");
        of13Helper->EnableDatapathStats ("switch-stats");
        csma.EnablePcap ("switch", switchPorts[0], true);
        csma.EnablePcap ("switch", switchPorts[1], true);
        csma.EnablePcap ("switch", switchPorts[2], true);
        csma.EnablePcap ("host", ueDev);
    }


    // Run the simulation
    AnimationInterface anim("test_LTE.xml");
    anim.EnablePacketMetadata();

    double x_size = 4.0;
    double y_size = 4.0;

    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (ueNodes.Get (i), "UE");
        anim.UpdateNodeSize (ueNodes.Get (i)->GetId(), x_size, y_size);
        anim.UpdateNodeColor (ueNodes.Get (i), 255, 0, 0);
    }
    for (uint32_t i = 0; i < src_sink; i++)
    {
        anim.UpdateNodeDescription (enbNodes.Get (i), "eNB");
	    anim.UpdateNodeSize (enbNodes.Get (i)->GetId(), x_size, y_size);
        anim.UpdateNodeColor (enbNodes.Get (i), 0, 255, 0);
    }

    anim.UpdateNodeDescription (pgw, "PGW");
    anim.UpdateNodeSize (pgw->GetId(), x_size, y_size);
    anim.UpdateNodeColor (pgw, 0, 0, 255);
    anim.UpdateNodeDescription (server, "server");
    anim.UpdateNodeSize (server->GetId(), x_size, y_size);
    anim.UpdateNodeColor (server, 0, 0, 255);
    for (uint32_t i = 0; i < numSwitches; i++)
    {
        anim.UpdateNodeDescription (switches.Get (i), "OFS");
	    anim.UpdateNodeSize (switches.Get (i)->GetId(), x_size, y_size);
        anim.UpdateNodeColor (switches.Get (i), 0, 255, 0);
    }
    anim.UpdateNodeDescription (controllers.Get (0), "OFC");
    anim.UpdateNodeSize (controllers.Get (0)->GetId(), x_size, y_size);
    anim.UpdateNodeColor (controllers.Get (0), 0, 0, 255);

    Simulator::Schedule (Seconds (1), modify);
    Simulator::Schedule (Seconds (1), AdvancePosition, ueNodes.Get (sourceNode));
    Simulator::Schedule (Seconds (1), AdvancePosition, ueNodes.Get (sinkNode));
    Simulator::Schedule (Seconds (1), AdvancePosition, switches.Get (sourceNode));
    Simulator::Schedule (Seconds (1), AdvancePosition, switches.Get (2));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (simTime);
    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";     //フローiの送信パケット数合計
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";	    //送信バイト数合計
        std::cout << "  Tx Offered: " << i->second.txBytes * 8.0 / 10 / 1000 / 1000  << " Mbps\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";	    //受信パケット数合計
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";	    //受信バイト数合計
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 10 / 1000 / 1000  << " Mbps\n";	//スループット
    }

    Simulator::Destroy ();
    return 0;
}


class TakumaController : public OFSwitch13Controller{
	protected:
		void HandshakeSuccessful(Ptr<const RemoteSwitch> swtch);
        ofl_err HandleFlowRemoved (struct ofl_msg_flow_removed *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid);
};


void TakumaController::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch)
{
    if((int)swtch->GetDpId()==1){
        printf("%d",(int)swtch->GetDpId());
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 apply:output=1");
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=3 in_port=1 apply:output=2");
    }
    if((int)swtch->GetDpId()==2){
        printf("%d",(int)swtch->GetDpId());
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,flags=0x0001,prio=2 in_port=2 write:output=3");
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,flags=0x0001,prio=1 in_port=3 write:output=2");
    }
    if((int)swtch->GetDpId()==3){
        printf("%d",(int)swtch->GetDpId());
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=0 write:output=1");
        DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=0 write:output=2");
    }
}

ofl_err TakumaController::HandleFlowRemoved (struct ofl_msg_flow_removed *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid)
{
    if((int)swtch->GetDpId()==1){
        if(j==1){
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=3 apply:output=1");
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 apply:output=3");
            j=0;
        }else{
            j=1;
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 apply:output=1");
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 apply:output=2");
        }
    }
    if((int)swtch->GetDpId()==3){
        if(i==1){
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=3 write:output=1");
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 write:output=3");
            i=0;
        }else{
            i=1;
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 write:output=1");
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 write:output=2");
        }
    }
    return 0;
}


