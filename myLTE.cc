/*
 * Network Topology
 * 
 *       P-GW *-----------* server
 *           / \
 *          /   \
 *         /     \
 *        /       \
 *   eNB /         \ eNB
 *      *-----------*
 *      |           |
 *      |           |
 *      *           *
 *      n0          n1
 * 
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


using namespace ns3;

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

NS_LOG_COMPONENT_DEFINE ("Study");

int
main (int argc, char *argv[])
{
    uint16_t numNodes = 7;
    uint16_t src_sink = 2;
    uint16_t sourceNode = 0;
    uint16_t sinkNode = numNodes - 1;
    Time simTime = Seconds (10.0);
    Time simStart = Seconds (1.0);
    Time simStop = Seconds (10.0);
    Time interPacketInterval = MilliSeconds (100);
    double distance = 200.0;    // m
    uint32_t packetSize = 1024; //byte

    // bool disableDl = false;
    // bool disableUl = false;
    // bool disablePl = false;
    

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
    cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
    cmd.AddValue ("distance", "Distance between nodes", distance);
    cmd.AddValue ("packetSize", "Size of packet", packetSize);
    // cmd.AddValue ("disableDl", "Disable downlink data flows", disableDl);
    // cmd.AddValue ("disableUl", "Disable uplink data flows", disableUl);
    // cmd.AddValue ("disablePl", "Disable data flows between peer UEs", disablePl);
    cmd.Parse (argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults ();

    // parse again so you can override default values from the command line
    cmd.Parse(argc, argv);

    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

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

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create (numNodes);
    enbNodes.Create (src_sink);

    //モビリティモデルの設定
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 0; i < numNodes; i++)
    {
        if (i % 3 == 0){
            positionAlloc->Add(Vector(distance * (i / 3), 200, 0));
        }
        else if (i % 3 == 1){
            positionAlloc->Add(Vector(distance * ((i - 1) / 3) + distance / 2, 170, 0));
        }
        else{
            positionAlloc->Add(Vector(distance * ((i - 2) / 3) + distance / 2, 230, 0));
        }
    }
    for (uint16_t i = 0; i < src_sink; i++)
    {
        positionAlloc->Add(Vector(distance * 2 * i + 50, 150, 0));
    }
    positionAlloc->Add(Vector(distance + 50, 120, 0));
    positionAlloc->Add(Vector(distance + 50, 80, 0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install (ueNodes);
    mobility.Install (enbNodes);
    mobility.Install (pgw);
    mobility.Install (server);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (NodeContainer(ueNodes.Get(sourceNode), ueNodes.Get(sinkNode)));

    // Install the IP stack on the UEs
    internet.Install (ueNodes);

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
    // for (uint16_t i = 0; i < numNodes; i++)
    // {
    //     std::cout << "IP address : " << ueIpIface.GetAddress(i) << "\n";
    // }

    /*
    // Install and start applications on UEs and remote host
    uint16_t dlPort = 1100;
    uint16_t ulPort = 2000;
    uint16_t otherPort = 3000;

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
        if (!disableDl)
        {
            PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
            serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));

            UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
            dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
            dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
            clientApps.Add (dlClient.Install (server));
        }

        if (!disableUl)
        {
            ++ulPort;
            PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
            serverApps.Add (ulPacketSinkHelper.Install (server));

            UdpClientHelper ulClient (serverAddr, ulPort);
            ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
            ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
            clientApps.Add (ulClient.Install (ueNodes.Get(u)));
        }
        if (!disablePl && numNodes > 1)
        {
            ++otherPort;
            PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
            serverApps.Add (packetSinkHelper.Install (ueNodes.Get (u)));

            UdpClientHelper client (ueIpIface.GetAddress (u), otherPort);
            client.SetAttribute ("Interval", TimeValue (interPacketInterval));
            client.SetAttribute ("MaxPackets", UintegerValue (1000000));
            clientApps.Add (client.Install (ueNodes.Get ((u + 1) % numNodes)));
        }
    }

    serverApps.Start (MilliSeconds (500));
    clientApps.Start (MilliSeconds (500));
    */

    UdpServerHelper Server (9);
    ApplicationContainer serverApps = Server.Install(ueNodes.Get(sinkNode));
    serverApps.Start (simStart);
    serverApps.Stop (simStop);

    UdpClientHelper Client(ueIpIface.GetAddress(sinkNode-5),9);
    Client.SetAttribute ("MaxPackets", UintegerValue (10000));
    Client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = Client.Install(ueNodes.Get(sourceNode));
    clientApps.Start (simStart);
    clientApps.Stop (simStop);

    lteHelper->EnableTraces ();
    // Uncomment to enable PCAP tracing
    // p2p.EnablePcapAll("lena-simple-epc-backhaul");


    AnimationInterface anim ("my-lte-ver7.xml"); // Mandatory

    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (ueNodes.Get (i), "UE"); // Optional
        anim.UpdateNodeColor (ueNodes.Get (i), 255, 0, 0); // Optional
    }
    for (uint32_t i = 0; i < src_sink; i++)
    {
        anim.UpdateNodeDescription (enbNodes.Get (i), "eNB"); // Optional
        anim.UpdateNodeColor (enbNodes.Get (i), 0, 255, 0); // Optional
    }

    anim.UpdateNodeDescription (pgw, "PGW"); // Optional
    anim.UpdateNodeColor (pgw, 0, 0, 255); // Optional
    anim.UpdateNodeDescription (server, "server"); // Optional
    anim.UpdateNodeColor (server, 0, 0, 255); // Optional


    anim.EnablePacketMetadata (); // Optional

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    for (int i = 0; i < src_sink; i++)
    {
        Simulator::Schedule (Seconds (1.0), AdvancePosition, ueNodes.Get (i*(numNodes-1)));
    }

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
