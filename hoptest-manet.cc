#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"



using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HopTest");

int
main (int argc, char *argv[])
{
    std::string phyMode ("VhtMcs1");
    uint16_t numNodes = 7;
    uint16_t sinkNode = numNodes - 1;    //destination
    uint16_t srcNode = 0;  //source
    double start = 1.0;
	double stop = 10.0;
    Time simTime = Seconds (stop);
    Time simStart = Seconds (start);
    Time simStop = Seconds (stop);
    Time interPacketInterval = MicroSeconds (156);
    double distance = 90.0;  // m
    double sinkPos = distance * 3 - 20;
    uint32_t packetSize = 1024; // bytes
    int no_manet = 1;
    int rss = -80;

    // Configure command line parameters
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
	cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
    cmd.AddValue ("distance", "Distance between nodes", distance);
    cmd.AddValue ("packetSize", "Size of packet", packetSize);
    cmd.Parse (argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults ();

    cmd.Parse(argc, argv);


    //LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    //LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    //LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    //LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

    // disable fragmentation for frames below 2200 bytes
    // Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
    // turn off RTS/CTS for frames below 2200 bytes
    // Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
    // Set non-unicast data rate to be the same as that of unicast
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));

    NodeContainer ueNodes;
    ueNodes.Create(numNodes);

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    // wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
    wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.Set ("RxGain", DoubleValue (0) );
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    // wifiPhy.Set("ChannelWidth", UintegerValue (channelwidth));
    // wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    // wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(90.1));
    wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
    wifiPhy.SetChannel (wifiChannel.Create ());

    // Add a non-QoS upper mac, and disable rate control (i.e. ConstantRateWifiManager)
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
    
    // Set it to adhoc mode
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, ueNodes);

    // not Good
    //Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(40));
   
    //Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue(true));


    // Configure mobility
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
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (ueNodes);

    AodvHelper aodv;
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;

    list.Add(staticRouting, 0);
    if (no_manet == 1){
        list.Add(aodv, 10);
    }else{
        list.Add(olsr, 10);
    }
    

    InternetStackHelper internet;
    internet.SetRoutingHelper (list); // has effect on the next Install ()
    internet.Install (ueNodes);

    Ipv4AddressHelper ipv4;
    NS_LOG_INFO ("Assign IP Addresses.");
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign (devices);

    UdpServerHelper Server (9);
    ApplicationContainer serverApps = Server.Install(ueNodes.Get(sinkNode));
    serverApps.Start (simStart);
    serverApps.Stop (simStop);

    UdpClientHelper Client(i.GetAddress(sinkNode),9);
    Client.SetAttribute ("MaxPackets", UintegerValue (1000000));
    Client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = Client.Install(ueNodes.Get(srcNode));
    clientApps.Start (simStart);
    clientApps.Stop (simStop);
/*
    uint16_t Port = 1234;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    // PacketSinkHelper PacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), Port));
    // serverApps.Add (PacketSinkHelper.Install (ueNodes.Get(sinkNode)));
    // BulkSendHelper Client ("ns3::TcpSocketFactory", InetSocketAddress (i.GetAddress (sinkNode), Port));
    // clientApps.Add (Client.Install (ueNodes.Get(srcNode)));
    OnOffHelper onOff ("ns3::UdpSocketFactory", InetSocketAddress (i.GetAddress (sinkNode), Port));
    onOff.SetConstantRate(DataRate("3Mbps"));
    onOff.SetAttribute ("PacketSize", UintegerValue (packetSize));
    clientApps = onOff.Install (ueNodes.Get (srcNode));

    PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), Port));
    serverApps = sink.Install (ueNodes.Get (sinkNode));

    serverApps.Start (simStart);
    serverApps.Stop (simStop);
    clientApps.Start (simStart);
    clientApps.Stop (simStop);
*/
    AnimationInterface anim ("adhoc-hop-test.xml");
    // anim.SetMaxPktsPerTraceFile(10000000);
    anim.EnablePacketMetadata ();

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
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Tx Offered: " << i->second.txBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";
    }

    Simulator::Destroy ();

    return 0;
}
