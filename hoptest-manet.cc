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



NS_LOG_COMPONENT_DEFINE ("WifiAdhoc");

// void ReceivePacket (Ptr<Socket> socket)
// {
//   while (socket->Recv ())
//     {
//       NS_LOG_UNCOND ("Received one packet!");
//     }
// }

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

int
main (int argc, char *argv[])
{
    //std::string phyMode ("DsssRate1Mbps");
    uint16_t numNodes = 6;
    uint16_t sinkNode = numNodes - 1;    //destination
    uint16_t sourceNode = 0;  //source
    double start = 1.0;
	double stop = 10.0;
    Time simTime = Seconds (10.0);
    Time simStart = Seconds (start);
    Time simStop = Seconds (stop);
    Time interPacketInterval = MilliSeconds (100);
    double distance = 90.0;  // m
    //uint32_t packetSize = 128; // bytes
    int no_manet = 1;
    double txPower = 100.0;

    //Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));

    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    
    
    NodeContainer wifiNodes;
    wifiNodes.Create(numNodes);

    // NodeContainer wifiPair[numNodes-1];
    // for (int i = 0; i < numNodes-1; i++)
    // {
    //     wifiPair[i].Add(wifiNodes.Get(i));
    //     wifiPair[i].Add(wifiNodes.Get(i+1));
    // }

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("ShortGuardEnabled", BooleanValue(1));
	wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
	wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));
    // wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-42.0));	// Default is -96dBm
    // wifiPhy.Set("CcaMode1Threshold", DoubleValue(-42.0));			// Default is -99dBm


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
    // wifiMac.SetType ("ns3::AdhocWifiMac");
    // for (int i = 0; i < numNodes-1; i++)
    // {
    //     NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, wifiPair[i]);
    // }
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, wifiNodes);

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(5));
    

    // Configure mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (int i = 0; i < numNodes; i++)
    {
/*
        if (i % 2 == 0)
        {
            positionAlloc->Add(Vector(distance * i, 150, 0));
        }else{
            positionAlloc->Add(Vector(distance * i, 180, 0));
        }
*/
        positionAlloc->Add(Vector(distance * i, 150, 0));
    }
/*
    for (int i = 0; i < numNodes; i++)
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
*/
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);

    AodvHelper aodv;
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting ;

    Ipv4ListRoutingHelper list ;

    list.Add(staticRouting, 0);
    if (no_manet == 1){
        list.Add(aodv, 10);
    }else{
        list.Add(olsr, 10);
    }
    

    InternetStackHelper internet;
    internet.SetRoutingHelper (list); // has effect on the next Install ()
    internet.Install (wifiNodes);

    Ipv4AddressHelper ipv4;
    NS_LOG_INFO ("Assign IP Addresses.");
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign (devices);

/*
    UdpServerHelper Server (9);
    ApplicationContainer serverApps = Server.Install(wifiNodes.Get(sinkNode));
    serverApps.Start (simStart);
    serverApps.Stop (simStop);

    UdpClientHelper Client(i.GetAddress(sinkNode),9);
    Client.SetAttribute ("MaxPackets", UintegerValue (10000000));
    Client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = Client.Install(wifiNodes.Get(sourceNode));
    clientApps.Start (simStart);
    clientApps.Stop (simStop);
*/
    uint16_t dlPort = 1234;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
    serverApps.Add (dlPacketSinkHelper.Install (wifiNodes.Get(sinkNode)));
    BulkSendHelper dlClient ("ns3::TcpSocketFactory", InetSocketAddress (i.GetAddress (sinkNode), dlPort));
    clientApps.Add (dlClient.Install (wifiNodes.Get(sourceNode)));

    serverApps.Start (simStart);
    serverApps.Stop (simStop);
    clientApps.Start (simStart);
    clientApps.Stop (simStop);
    // Simulator::Schedule (Seconds (1), modify);
    // for (int i = 0; i < numNodes; i++)
    // {
    //     Simulator::Schedule (Seconds (1), AdvancePosition, wifiNodes.Get (i));
    // }

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
