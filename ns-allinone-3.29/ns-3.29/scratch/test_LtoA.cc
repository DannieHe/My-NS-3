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
#include "ns3/aodv-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Study");

void BandWidthTrace1()
{
    Config::Set("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/DataRate", StringValue("27Mbps"));
}
void BandWidthTrace2()
{
    Config::Set("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/DataRate", StringValue("18Mbps"));
}
void BandWidthTrace3()
{
    Config::Set("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/DataRate", StringValue("9Mbps"));
}

int
main (int argc, char *argv[])
{
    uint16_t numNodes = 7;
    uint16_t srcNode = 0;
    uint16_t sinkNode = numNodes - 1;
	double start = 1.0;
	double middle = 3000;
    double stop = 30.0;
    Time simTime = Seconds (stop);
    Time simStart1 = Seconds (start);
    Time simStop1 = MilliSeconds (middle);
    Time simStart2 = MilliSeconds (middle);
    Time simStop2 = Seconds (stop);
    double distance = 100.0;    // m
    double sinkPos = distance * 4 - 20;
    uint32_t packetSize = 1500; //byte

    // Configure command line parameters
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
    cmd.AddValue ("distance", "Distance between nodes", distance);
    cmd.AddValue ("packetSize", "Size of packet", packetSize);
    cmd.Parse (argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults ();

    cmd.Parse(argc, argv);

    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::LteEnbNetDevice::UlBandwidth", UintegerValue(25));
    Config::SetDefault("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue(25));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(1024 * 1024));

    // Create UE node
    NodeContainer ueNodes;
    ueNodes.Create (numNodes);

    // Add LTE node
    NodeContainer lteNodes;
    lteNodes.Create (2);

    // Create eNB node
    NodeContainer enbNodes;
    enbNodes.Create (2);


    /*
     *      LTE
     */
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<EpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    
    lteHelper->SetEpcHelper (epcHelper);

    // Create PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    // Create server
    NodeContainer serverContainer;
    serverContainer.Create (1);
    Ptr<Node> server = serverContainer.Get (0);
    InternetStackHelper internet;
    internet.Install (serverContainer);

    // Create p2p
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

	/*
	 *	Ad Hoc
	 */
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100.1));
    // wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
    //                                  "Exponent", DoubleValue (3.0));
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
        
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue ("HtMcs7"),
                                "ControlMode",StringValue ("HtMcs1"));

    // Set it to adhoc mode
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer adhocDev = wifi.Install (wifiPhy, wifiMac, ueNodes);


    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	for (int i = 0; i < numNodes; i++)
    {
        if (i == numNodes - 1)
        {
            positionAlloc->Add(Vector(sinkPos, 160, 0));
        }else{
            positionAlloc->Add(Vector(distance * i, 150, 0));
        }
    }
    for (int i = 0; i < 2; i++)
    {
        positionAlloc->Add(Vector(distance * i , 140, 0));
    }
    positionAlloc->Add(Vector(0, 150, 0));
    positionAlloc->Add(Vector(sinkPos, 160, 0));
    positionAlloc->Add(Vector(distance + 50, 120, 0));
    positionAlloc->Add(Vector(distance + 50, 80, 0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install (ueNodes);
    mobility.Install (enbNodes);
    mobility.Install (lteNodes);
    mobility.Install (pgw);
    mobility.Install (server);


    /*
     *          LTE
     */
    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (lteNodes);

    // Install the IP stack on the UEs
    internet.Install(lteNodes);

    Ipv4InterfaceContainer lteIpIface;
    lteIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint16_t u = 0; u < 2; u++)
    {
        Ptr<Node> lteNode = lteNodes.Get (u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> lteStaticRouting = ipv4RoutingHelper.GetStaticRouting (lteNode->GetObject<Ipv4> ());
        lteStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < 2; i++)
    {
        lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
        // side effect: the default EPS bearer will be activated
    }

	/*
	 *	Ad Hoc
	 */
	AodvHelper aodv;
    Ipv4StaticRoutingHelper adhocStaticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(adhocStaticRouting, 0);
	list.Add(aodv, 10);
    
    InternetStackHelper stack;
    stack.SetRoutingHelper (list); // has effect on the next Install ()
    stack.Install (ueNodes);
    
    Ipv4AddressHelper address;
    NS_LOG_INFO ("Assign IP Addresses.");
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer adhocIpIface = address.Assign (adhocDev);


    // Simulator::Schedule (Seconds(2) , &BandWidthTrace1);
    // Simulator::Schedule (Seconds(3) , &BandWidthTrace2);
    // Simulator::Schedule (Seconds(4) , &BandWidthTrace3);


    // LTE
    uint16_t ltePort = 20;
    OnOffHelper LteClient ("ns3::TcpSocketFactory", InetSocketAddress (lteIpIface.GetAddress (1), ltePort));
    LteClient.SetAttribute ("MaxBytes", UintegerValue (9000000));
    LteClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
    LteClient.SetAttribute ("DataRate", StringValue ("35Mbps"));
    ApplicationContainer LteclientApps = LteClient.Install (lteNodes.Get (0));
    LteClientApps.Start (simStart1);
    LteClientApps.Stop (simStop1);

    PacketSinkHelper LteServer ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ltePort));
    ApplicationContainer LteServerApps = LteServer.Install (lteNodes.Get(1));
    LteServerApps.Start (simStart1);
    LteServerApps.Stop (simStop1);


    lteHelper->EnableTraces ();


    // adhoc
    uint16_t adhocPort = 10;
    BulkSendHelper AdhocClient ("ns3::TcpSocketFactory", InetSocketAddress (adhocIpIface.GetAddress (sinkNode), adhocPort));
    AdhocClient.SetAttribute ("MaxBytes", UintegerValue (12582964));
    AdhocClient.SetAttribute ("SendSize", UintegerValue (packetSize));
    ApplicationContainer AdhocClientApps = AdhocClient.Install (ueNodes.Get(srcNode));
    AdhocClientApps.Start (simStart2);
    AdhocClientApps.Stop (simStop2);

    PacketSinkHelper AdhocServer ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), adhocPort));
    ApplicationContainer AdhocServerApps = AdhocServer.Install (ueNodes.Get(sinkNode));
    AdhocServerApps.Start (simStart2);
    AdhocServerApps.Stop (simStop2);


    // Run the simulation
    AnimationInterface anim("test_LtoA.xml");
    anim.SetMaxPktsPerTraceFile(100000000);
    anim.EnablePacketMetadata();

    double x_size = 4.0;
    double y_size = 4.0;

    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (ueNodes.Get (i), "UE");
        anim.UpdateNodeSize (ueNodes.Get (i)->GetId(), x_size, y_size);
        anim.UpdateNodeColor (ueNodes.Get (i), 255, 0, 0);
    }
    for (uint32_t i = 0; i < 2; i++)
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
        if (t.sourceAddress == "7.0.0.2"){
            std::cout << "  Tx Offered(LTE): " << i->second.txBytes * 8.0 / ((middle / 1000) - start) / 1000 / 1000  << " Mbps\n";
        }else{
            std::cout << "  Tx Offered: " << i->second.txBytes * 8.0 / (stop - (middle / 1000)) / 1000 / 1000  << " Mbps\n";
        }
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";	    //受信パケット数合計
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";	    //受信バイト数合計
        if (t.sourceAddress == "7.0.0.2"){
            std::cout << "  Throughput(LTE): " << i->second.rxBytes * 8.0 / ((middle / 1000) - start) / 1000 / 1000  << " Mbps\n";	//スループット
        }else{
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (stop - (middle / 1000)) / 1000 / 1000  << " Mbps\n";
        }
        std::cout << "  SumofDelay  " << i->second.delaySum << "\n";
    }

    Simulator::Destroy ();
    return 0;
}
