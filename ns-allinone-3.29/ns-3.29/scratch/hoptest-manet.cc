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
    // std::string phyMode = "HtMcs1";
    uint16_t numNodes = 7;
    uint16_t sinkNode = numNodes - 1;    //destination
    uint16_t srcNode = 0;  //source
    double start = 1.0;
	double stop = 10.0;
    Time simTime = Seconds (stop);
    Time simStart = Seconds (start);
    Time simStop = Seconds (stop);
    double distance = 100.0;  // m
    double sinkPos = distance * 4 - 20;
    uint32_t packetSize = 1500; // bytes
    int no_manet = 1;
    int channelwidth = 160;
    //int rss = -80;

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
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    //LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    //LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

/*
    // disable fragmentation for frames below 2200 bytes
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
    // turn off RTS/CTS for frames below 2200 bytes
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
    // Set non-unicast data rate to be the same as that of unicast
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
*/

    NodeContainer ueNodes;
    ueNodes.Create(numNodes);

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
    // wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
    // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
    wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("ChannelWidth", UintegerValue(channelwidth));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100.1));
    //wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
    wifiPhy.SetChannel (wifiChannel.Create ());

    // Add a non-QoS upper mac, and disable rate control (i.e. ConstantRateWifiManager)
    WifiMacHelper wifiMac;
    
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue ("HtMcs7"),
                                "ControlMode",StringValue ("HtMcs1"));
    
    
    // Set it to adhoc mode
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, ueNodes);


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

    uint16_t dlPort = 1234;
    BulkSendHelper Client ("ns3::TcpSocketFactory", InetSocketAddress (i.GetAddress (sinkNode), dlPort));
    Client.SetAttribute ("MaxBytes", UintegerValue (0));
    Client.SetAttribute ("SendSize", UintegerValue (1500));
    ApplicationContainer clientApps = Client.Install (ueNodes.Get(srcNode));
    clientApps.Start (simStart);
    clientApps.Stop (simStop);

    PacketSinkHelper Server ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
    ApplicationContainer serverApps = Server.Install (ueNodes.Get(sinkNode));
    serverApps.Start (simStart);
    serverApps.Stop (simStop);


    AnimationInterface anim ("adhoc-hop-test.xml");
    anim.SetMaxPktsPerTraceFile(10000000);
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
