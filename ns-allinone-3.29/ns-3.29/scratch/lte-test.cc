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
    uint16_t numNodes = 2;
    uint16_t sourceNode = 0;
    uint16_t sinkNode = numNodes - 1;
    double start = 1.0;
	double stop = 10.0;
    Time simTime = Seconds (10.0);
    Time simStart = Seconds (start);
    Time simStop = Seconds (stop);
    Time interPacketInterval = MilliSeconds (100);
    double distance = 200.0;    // m
    uint32_t packetSize = 1024; //byte

    // Command line arguments
    CommandLine cmd;
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

    //LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    //LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

    //Config::SetDefault("ns3::LteEnbNetDevice::UlBandwidth", UintegerValue(100));
    //Config::SetDefault("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue(100));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(1024 * 1024));

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
    enbNodes.Create (numNodes);

    //モビリティモデルの設定
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 0; i < numNodes; i++)
    {
        positionAlloc->Add(Vector(distance * i, 180, 0));
    }
    for (uint16_t i = 0; i < numNodes; i++)
    {
        positionAlloc->Add(Vector(distance * i, 150, 0));
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
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    // Install the IP stack on the UEs
    internet.Install (ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint16_t u = 0; u < numNodes; u++)
    {
        Ptr<Node> ueNode = ueNodes.Get (u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < numNodes; i++)
    {
        lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
    }
/*
    UdpServerHelper Server (9);
    ApplicationContainer serverApps = Server.Install(ueNodes.Get(sinkNode));
    serverApps.Start (simStart);
    serverApps.Stop (simStop);

    UdpClientHelper Client(ueIpIface.GetAddress(sinkNode),9);
    Client.SetAttribute ("MaxPackets", UintegerValue (10000));
    Client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = Client.Install(ueNodes.Get(sourceNode));
    clientApps.Start (simStart);
    clientApps.Stop (simStop);
*/
    uint16_t dlPort = 1234;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;
/*
    PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
    serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(sinkNode)));
    BulkSendHelper dlClient ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIface.GetAddress (sinkNode), dlPort));
    clientApps.Add (dlClient.Install (ueNodes.Get(sourceNode)));
*/
    OnOffHelper onOff ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIface.GetAddress (sinkNode), dlPort));
    onOff.SetConstantRate(DataRate("1Mbps"));
    onOff.SetAttribute ("PacketSize", UintegerValue (packetSize));
    clientApps = onOff.Install (ueNodes.Get (sourceNode));

    PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
    serverApps = sink.Install (ueNodes.Get (sinkNode));


    serverApps.Start (simStart);
    serverApps.Stop (simStop);
    clientApps.Start (simStart);
    clientApps.Stop (simStop);

    lteHelper->EnableTraces ();


    AnimationInterface anim ("lte-test.xml"); // Mandatory

    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (ueNodes.Get (i), "UE"); // Optional
        anim.UpdateNodeColor (ueNodes.Get (i), 255, 0, 0); // Optional
    }
    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (enbNodes.Get (i), "eNB"); // Optional
        anim.UpdateNodeColor (enbNodes.Get (i), 0, 255, 0); // Optional
    }

    anim.UpdateNodeDescription (pgw, "PGW"); // Optional
    anim.UpdateNodeColor (pgw, 0, 0, 255); // Optional
    anim.UpdateNodeDescription (server, "server"); // Optional
    anim.UpdateNodeColor (server, 0, 0, 255); // Optional

    anim.SetMaxPktsPerTraceFile(10000000);
    anim.EnablePacketMetadata (); // Optional

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // for (int i = 0; i < numNodes; i++)
    // {
    //     Simulator::Schedule (Seconds (1.0), AdvancePosition, ueNodes.Get (i*(numNodes-1)));
    // }

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
        std::cout << "  Tx Offered: " << i->second.txBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";	    //受信パケット数合計
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";	    //受信バイト数合計
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";	//スループット
    }

    Simulator::Destroy ();
    return 0;
}
