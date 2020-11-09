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
    std::string phyMode = "DsssRate1Mbps";

    uint16_t numNodes = 7;
    uint16_t srcNode = 0;
    uint16_t sinkNode = numNodes - 1;
    double start = 1.0;
	double stop = 60.0;
    Time simTime = Seconds (60.0);
    Time simStart = Seconds (start);
    Time simStop = Seconds (stop);
    Time AdhocInterval = MicroSeconds (6560);
    double distance = 90.0;    // m
    double sinkPos = distance * 1 - 20;
    uint32_t packetSize = 1500; //byte
    int channelwidth = 40;

    // Configure command line parameters
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
	cmd.AddValue ("AdhocInterval", "Inter packet interval", AdhocInterval);
    cmd.AddValue ("distance", "Distance between nodes", distance);
    cmd.AddValue ("packetSize", "Size of packet", packetSize);
    cmd.Parse (argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults ();

    cmd.Parse(argc, argv);

    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);


    // disable fragmentation for frames below 2200 bytes
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
    // turn off RTS/CTS for frames below 2200 bytes
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
    // Set non-unicast data rate to be the same as that of unicast
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));

    // Create UE node
    NodeContainer ueNodes;
    ueNodes.Create (numNodes);

	/*
	 *	Ad Hoc
	 */
    WifiHelper wifi;
    //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("ChannelWidth", UintegerValue (channelwidth));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(90.1)); 
    // wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
    //                                 "Exponent", DoubleValue (3.0));
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;

    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode",StringValue (phyMode),
                               "ControlMode",StringValue (phyMode));

    // Set it to adhoc mode
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer adhocDev = wifi.Install (wifiPhy, wifiMac, ueNodes);


    MobilityHelper mobility;

	mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
	mobility.Install(ueNodes);
	for (int i = 0; i < numNodes; i++)
	{
		Ptr<ConstantVelocityMobilityModel> ue_mob = ueNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
		if (i == numNodes - 1){
			ue_mob->SetPosition(Vector(sinkPos, 160, 0));
            ue_mob->SetVelocity(Vector(5.0, 0.0, 0.0));
        }else{
			ue_mob->SetPosition(Vector(distance * i, 150, 0));
		    ue_mob->SetVelocity(Vector(0.0, 0.0, 0.0));
        }
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


    // adhoc
    UdpServerHelper AdhocServer (9);
    ApplicationContainer AdhocServerApps = AdhocServer.Install(ueNodes.Get(sinkNode));
    AdhocServerApps.Start (simStart);
    AdhocServerApps.Stop (simStop);

    UdpClientHelper AdhocClient(adhocIpIface.GetAddress(sinkNode),9);
    AdhocClient.SetAttribute ("MaxPackets", UintegerValue (1000));
    AdhocClient.SetAttribute ("Interval", TimeValue (AdhocInterval));
    AdhocClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer AdhocClientApps = AdhocClient.Install(ueNodes.Get(srcNode));
    AdhocClientApps.Start (simStart);
    AdhocClientApps.Stop (simStop);
    /*
    uint16_t adhocPort = 1234;
    OnOffHelper onOff2 ("ns3::TcpSocketFactory", InetSocketAddress (adhocIpIface.GetAddress (sinkNode), adhocPort));
    onOff2.SetConstantRate(DataRate("3Mbps"));
    onOff2.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer adhocClientApps = onOff2.Install (ueNodes.Get (srcNode));
    adhocClientApps.Start (simStart2);
    adhocClientApps.Stop (simStop2);

    PacketSinkHelper sink2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), adhocPort));
    ApplicationContainer adhocServerApps = sink2.Install (ueNodes.Get (sinkNode));
    adhocServerApps.Start (simStart2);
    adhocServerApps.Stop (simStop2);
    */

    // Run the simulation
    AnimationInterface anim("my-adhoc-ver6.xml");
    anim.EnablePacketMetadata();

    double x_size = 4.0;
    double y_size = 4.0;

    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (ueNodes.Get (i), "UE");
        anim.UpdateNodeSize (ueNodes.Get (i)->GetId(), x_size, y_size);
        anim.UpdateNodeColor (ueNodes.Get (i), 255, 0, 0);
    }

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
