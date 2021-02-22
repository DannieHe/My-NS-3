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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAdhocTest");


int main (int argc, char *argv[])
{
    //std::string phyMode ("OfdmRate12Mbps");

    Time simTime = Seconds(1000.0);
    Time simStart = Seconds(1.0);
    Time simStop = Seconds(1000.0);
    Time interPacketInterval = MilliSeconds (10);
    double distance =105.0;    // m
    uint32_t packetSize = 1024; //byte

    NodeContainer wifiNodes;
    wifiNodes.Create (2);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    // ns-3 supports RadioTap and Prism tracing extensions for 802.11
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

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
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, wifiNodes);

    // Configure mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (distance, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);


    InternetStackHelper internet;
    internet.Install (wifiNodes);

    Ipv4AddressHelper ipv4;
    NS_LOG_INFO ("Assign IP Addresses.");
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign (devices);

    UdpServerHelper Server (9);
    ApplicationContainer serverApps = Server.Install(wifiNodes.Get(1));
    serverApps.Start (simStart);
    serverApps.Stop (simStop);

    UdpClientHelper Client(i.GetAddress(1),9);
    Client.SetAttribute ("MaxPackets", UintegerValue (10000000));
    Client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = Client.Install(wifiNodes.Get(0));
    clientApps.Start (simStart);
    clientApps.Stop (simStop);

    // Tracing
    wifiPhy.EnablePcap ("wifi-simple-adhoc", devices);


    AnimationInterface anim ("my-wifi-simple-adhoc.xml"); // Mandatory
    anim.SetMaxPktsPerTraceFile(10000000);
    anim.EnablePacketMetadata (); // Optional

    Simulator::Stop (simTime);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

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
