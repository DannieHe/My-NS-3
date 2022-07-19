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
    // uint16_t src_sink = 2;
    // uint16_t sourceNode = 0;
    // uint16_t sinkNode = numNodes - 1;
    Time simTime = Seconds (30.0);
    Time simStart = Seconds (1.0);
    Time simStop = Seconds (30.0);
    Time interPacketInterval = MilliSeconds (100);
    double distance = 200.0;    // m
    uint32_t packetSize = 1024; // byte

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

    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);


    NodeContainer ueNodes;      // = 7
    ueNodes.Create (numNodes);
    NodeContainer apNodes;      // = 1
    apNodes.Create (2);

    NetDeviceContainer wifiDev;
    NetDeviceContainer apDev;

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
	  YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    // wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
    // wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
    //                         "DataMode",StringValue ("HtMcs0"),
    //                         "ControlMode",StringValue ("HtMcs7"));

    WifiMacHelper mac;
    Ssid ssid = Ssid ("Wifi");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    wifiDev = wifi.Install(phy, mac, ueNodes.Get(0));

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    apDev = wifi.Install(phy, mac, apNodes);
    

    //モビリティモデルの設定
    MobilityHelper mobility;
    // Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    // for (uint16_t i = 0; i < numNodes; i++)
    // {
    //     if (i % 3 == 0){
    //         positionAlloc->Add(Vector(distance * (i / 3), 210, 0));
    //     }
    //     else if (i % 3 == 1){
    //         positionAlloc->Add(Vector(distance * ((i - 1) / 3) + distance / 2, 180, 0));
    //     }
    //     else{
    //         positionAlloc->Add(Vector(distance * ((i - 2) / 3) + distance / 2, 240, 0));
    //     }
    // }
    // for (uint16_t i = 0; i < src_sink; i++)
    // {
    //     positionAlloc->Add(Vector(distance * 2 * i + 50, 140, 0));
    // }
    // mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install (ueNodes);
    mobility.Install (apNodes);

    
    //プロトコルスタックの設定（IPv4かIPv6か決める）
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(apNodes);

    //IPアドレス割り当て
    //右側のノード、AP
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    // Ipv4InterfaceContainer p2pInterface;
    // p2pInterface = address.Assign (p2pDev);
    // address.SetBase ("10.1.2.0", "255.255.255.0");
    // Ipv4InterfaceContainer csmaInterface;
    // csmaInterface = address.Assign (csmaDev);
    // address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiIpIface;
    wifiIpIface = address.Assign (wifiDev);
    Ipv4InterfaceContainer apIpIface;
    apIpIface = address.Assign (apDev);



    UdpServerHelper Server (9);
    ApplicationContainer serverApps = Server.Install(ueNodes.Get(1));
    serverApps.Start (simStart);
    serverApps.Stop (simStop);

    UdpClientHelper Client(wifiIpIface.GetAddress(0),9);
    Client.SetAttribute ("MaxPackets", UintegerValue (10000));
    Client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = Client.Install (ueNodes.Get (0));
    clientApps.Start (simStart);
    clientApps.Stop (simStop);

    AnimationInterface anim ("my-wifi-ver9.xml"); // Mandatory

    // for (uint32_t i = 0; i < numNodes; i++)
    // {
    //     anim.UpdateNodeDescription (ueNodes.Get (i), "UE"); // Optional
    //     anim.UpdateNodeColor (ueNodes.Get (i), 255, 0, 0); // Optional
    // }
    // for (uint32_t i = 0; i < src_sink; i++)
    // {
    //     anim.UpdateNodeDescription (p2pNodes.Get (i), "AP"); // Optional
    //     anim.UpdateNodeColor (p2pNodes.Get (i), 0, 255, 0); // Optional
    // }


    anim.EnablePacketMetadata (); // Optional

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // for (int i = 0; i < src_sink; i++)
    // {
    //     Simulator::Schedule (Seconds (1), AdvancePosition, ueNodes.Get (i*(numNodes-1)));
    // }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
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
        std::cout << "  Tx Offered: " << i->second.txBytes * 8.0 / 10 / 1000 / 1000  << " Mbps\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 10 / 1000 / 1000  << " Mbps\n";
      }


    Simulator::Destroy ();
    return 0;
}
