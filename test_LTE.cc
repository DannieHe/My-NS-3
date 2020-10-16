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
#include <string>
#include <regex>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Study");
    
int
main (int argc, char *argv[])
{
    uint16_t numNodes = 7;
    uint16_t srcNode = 0;
    uint16_t sinkNode = numNodes - 1;
	double start = 1.0;
	double middle = 15.0;
	double stop = 30.0;
    Time simTime = Seconds (30.0);
    Time simStart1 = Seconds (start);
    Time simStop1 = Seconds (middle);
    Time simStart2 = Seconds (middle);
    Time simStop2 = Seconds (stop);
    Time LteInterval = MilliSeconds (1.64);
	Time AdhocInterval = MilliSeconds (6.56);
    double distance = 90.0;    // m
    uint32_t packetSize = 1024; //byte

    // Configure command line parameters
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
    cmd.AddValue ("LteInterval", "Inter packet interval", LteInterval);
	cmd.AddValue ("AdhocInterval", "Inter packet interval", AdhocInterval);
    cmd.AddValue ("distance", "Distance between nodes", distance);
    cmd.AddValue ("packetSize", "Size of packet", packetSize);
    cmd.Parse (argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults ();

    // parse again so you can override default values from the command line
    cmd.Parse(argc, argv);

    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Enable checksum computations (required by OFSwitch13 module)
    // GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));


    // Create the UE node
    NodeContainer ueNodes;
    ueNodes.Create (numNodes);

    NodeContainer lteNodes;
    lteNodes.Add(ueNodes.Get(srcNode));
    lteNodes.Add(ueNodes.Get(sinkNode));

    // Create the eNB node
    NodeContainer enbNodes;
    enbNodes.Create (2);


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

	/*
	 *	Ad Hoc
	 */
	// The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    //wifiPhy.Set("ChannelWidth", UintegerValue (channelwidth));
    //wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    //wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue (3.0));
    wifiPhy.SetChannel (wifiChannel.Create ());

    // Add a non-QoS upper mac, and disable rate control (i.e. ConstantRateWifiManager)
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue ("HtMcs0"),
                                "ControlMode",StringValue ("HtMcs7"));
    // Set it to adhoc mode
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer adhocDev = wifi.Install (wifiPhy, wifiMac, ueNodes);

    //Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(40));
   
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue(true));



    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	for (int i = 0; i < numNodes; i++)
    {
        if (i == numNodes - 1)
        {
            positionAlloc->Add(Vector(distance * 1 - 20, 160, 0));
        }else{
            positionAlloc->Add(Vector(distance * i, 150, 0));
        }
    }
    for (uint16_t i = 0; i < 2; i++)
    {
        positionAlloc->Add(Vector(distance * i , 140, 0));
    }
    positionAlloc->Add(Vector(distance + 50, 120, 0));
    positionAlloc->Add(Vector(distance + 50, 80, 0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install (ueNodes);
    mobility.Install (enbNodes);
    mobility.Install (pgw);
    mobility.Install (server);
/*
	mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
	mobility.Install(ueNodes);
	for (int i = 0; i < numNodes; i++)
	{
		Ptr<ConstantVelocityMobilityModel> ue_mob = ueNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
		if (i % 3 == 0){
			ue_mob->SetPosition(Vector(distance * (i / 3), 200, 0));
        }
        else if (i % 3 == 1){
			ue_mob->SetPosition(Vector(distance * ((i - 1) / 3) + distance / 2, 170, 0));
        }
        else{
			ue_mob->SetPosition(Vector(distance * ((i - 2) / 3) + distance / 2, 230, 0));
        }
		ue_mob->SetVelocity(Vector(10.0, 0.0, 0.0));   
	}
*/


    /*
     *          LTE
     */
    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    //NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (NodeContainer(ueNodes.Get(srcNode), ueNodes.Get(sinkNode)));
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (lteNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);

    Ipv4InterfaceContainer lteIpIface;
    lteIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint16_t u = 0; u < 2; u++)
    {
        //Ptr<Node> ueNode = ueNodes.Get (u*(numNodes-1));
        Ptr<Node> ueNode = lteNodes.Get (u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> lteStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
        lteStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < 2; i++)
    {
        lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
        // side effect: the default EPS bearer will be activated
    }

    Ipv4AddressHelper address;
    NS_LOG_INFO ("Assign IP Addresses.");
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer adhocIpIface = address.Assign (adhocDev);

    // LTE
    UdpServerHelper LteServer (20);
    ApplicationContainer LteServerApps = LteServer.Install(lteNodes.Get(1));
    LteServerApps.Start (simStart1);
    LteServerApps.Stop (simStop1);

    UdpClientHelper LteClient(lteIpIface.GetAddress(1),20);
    LteClient.SetAttribute ("MaxPackets", UintegerValue (100000));
    LteClient.SetAttribute ("Interval", TimeValue (LteInterval));
    LteClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer LteClientApps = LteClient.Install(lteNodes.Get(0));
    LteClientApps.Start (simStart1);
    LteClientApps.Stop (simStop1);

    lteHelper->EnableTraces ();

	/*
	 *	Ad Hoc
	 */
	AodvHelper aodv;
    Ipv4StaticRoutingHelper adhocStaticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(adhocStaticRouting, 0);
	list.Add(aodv, 10);
    
    //InternetStackHelper stack;
    //stack.SetRoutingHelper (list); // has effect on the next Install ()
    //stack.Install (ueNodes);

    // 分けて書くことで，正しいアドレスに
    internet.SetRoutingHelper (list);

    // adhoc
    UdpServerHelper AdhocServer (9);
    ApplicationContainer AdhocServerApps = AdhocServer.Install(ueNodes.Get(sinkNode));
    AdhocServerApps.Start (simStart2);
    AdhocServerApps.Stop (simStop2);

    UdpClientHelper AdhocClient(adhocIpIface.GetAddress(sinkNode),9);
    AdhocClient.SetAttribute ("MaxPackets", UintegerValue (100000));
    AdhocClient.SetAttribute ("Interval", TimeValue (AdhocInterval));
    AdhocClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer AdhocClientApps = AdhocClient.Install(ueNodes.Get(srcNode));
    AdhocClientApps.Start (simStart2);
    AdhocClientApps.Stop (simStop2);


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
        std::cout << "  Tx Offered: " << i->second.txBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";	    //受信パケット数合計
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";	    //受信バイト数合計
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";	//スループット
    }

    Simulator::Destroy ();
    return 0;
}
