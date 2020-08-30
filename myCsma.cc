/*
 *
 * /ns-3.29/my-example.ccより
 * 
 * ofswitch13使用
 * 
 */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>
#include <ns3/netanim-module.h>
#include <ns3/applications-module.h>
#include <ns3/flow-monitor-module.h>
#include <ns3/mobility-module.h>
#include <ns3/socket.h>
#include <string>
int i=0;
int j=0;

#define SIMSTART 1.0
#define SIMSTOP 11.0

using namespace ns3;

class Controller0;

void modify ()
{
    std::ostringstream oss;
    oss << "Update:" << Simulator::Now ().GetSeconds ();
    // Every update change the node description for node 2
    std::ostringstream node0Oss;
    node0Oss << "-----Node:" << Simulator::Now ().GetSeconds ();
    if (Simulator::Now ().GetSeconds () < 10) // This is important or the simulation
    {
        // will run endlessly
        Simulator::Schedule (Seconds (1), modify);
    }
}
    
int
main (int argc, char *argv[])
{
    uint16_t simTime = 10;
    bool verbose = false;
    bool trace = false;
    double distance = 200.0;  // m
    uint16_t packetSize = 1000; // bytes
    double packet_rate = 100;	// packet/s
    uint16_t numNodes = 7;
    uint16_t sinkNode = numNodes - 1;    //destination
    uint16_t sourceNode = 0;  //source

    // Configure command line parameters
    CommandLine cmd;
    cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
    cmd.AddValue ("verbose", "Enable verbose output", verbose);
    cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
    cmd.Parse (argc, argv);

    if (verbose)
    {
        OFSwitch13Helper::EnableDatapathLogs ();
        LogComponentEnable ("OFSwitch13Interface", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Device", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Port", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Queue", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13SocketHandler", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13LearningController", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
        LogComponentEnable ("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
    }

    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Enable checksum computations (required by OFSwitch13 module)
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

    // Create two host nodes
    NodeContainer hosts;
    hosts.Create (numNodes);

    // Create the switch node
    NodeContainer switches;
    switches.Create(numNodes);

    // Use the CsmaHelper to connect host nodes to the switch node
    CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
    csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

    NetDeviceContainer hostDevices;
    NetDeviceContainer switchPorts[numNodes];
    for (uint32_t i = 0; i < hosts.GetN (); i++)
    {
        NodeContainer pair (hosts.Get (i), switches.Get(i));
        NetDeviceContainer link = csmaHelper.Install (pair);
        hostDevices.Add (link.Get (0));
        switchPorts[i].Add (link.Get (1));
    }

    for (int i = 1; i < 4; i++)
    {
        NodeContainer pair1 (switches.Get (sourceNode), switches.Get(i));
        NetDeviceContainer link1 = csmaHelper.Install (pair1);
        switchPorts[sourceNode].Add (link1.Get (0));
        switchPorts[i].Add (link1.Get (1));
    }
    for (int i = 1; i < 6; i++)
    {
        if (i == 3) continue;
        NodeContainer pair2 (switches.Get (3), switches.Get(i));
        NetDeviceContainer link2 = csmaHelper.Install (pair2);
        switchPorts[3].Add (link2.Get (0));
        switchPorts[i].Add (link2.Get (1));
    }
    for (int i = 3; i < 6; i++)
    {
        NodeContainer pair3 (switches.Get (sinkNode), switches.Get(i));
        NetDeviceContainer link3 = csmaHelper.Install (pair3);
        switchPorts[sinkNode].Add (link3.Get (0));
        switchPorts[i].Add (link3.Get (1));
    }
    NodeContainer pair4 (switches.Get (1), switches.Get(4));
    NetDeviceContainer link4 = csmaHelper.Install (pair4);
    switchPorts[1].Add (link4.Get (0));
    switchPorts[4].Add (link4.Get (1));
    NodeContainer pair5 (switches.Get (2), switches.Get(5));
    NetDeviceContainer link5 = csmaHelper.Install (pair5);
    switchPorts[2].Add (link5.Get (0));
    switchPorts[5].Add (link5.Get (1));

    // Create two controller nodes
    NodeContainer controllers;
    controllers.Create (1);


    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (int i = 0; i < numNodes; i++)
    {
        if (i % 3 == 0){
            positionAlloc->Add(Vector(distance * (i / 3), 150, 0));
        }
        else if (i % 3 == 1){
            positionAlloc->Add(Vector(distance * ((i - 1) / 3) + distance / 2, 120, 0));
        }
        else{
            positionAlloc->Add(Vector(distance * ((i - 2) / 3) + distance / 2, 180, 0));
        }
    }
    for (int i = 0; i < numNodes; i++)
    {
        if (i % 3 == 0){
            positionAlloc->Add(Vector(distance * (i / 3), 160, 0));
        }
        else if (i % 3 == 1){
            positionAlloc->Add(Vector(distance * ((i - 1) / 3) + distance / 2, 130, 0));
        }
        else{
            positionAlloc->Add(Vector(distance * ((i - 2) / 3) + distance / 2, 190, 0));
        }
    }
    positionAlloc->Add(Vector(distance * 2, 50, 0));

    mobility.SetPositionAllocator(positionAlloc);
    // mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install (switches);
    mobility.Install (hosts);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install (controllers);
    for (NodeContainer::Iterator i = switches.Begin (); i != switches.End (); ++i){
        Ptr<Node> node = (*i);
        node->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10,0,0));
    }
    for (NodeContainer::Iterator j = hosts.Begin (); j != hosts.End (); ++j){
        Ptr<Node> node = (*j);
        node->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10,0,0));
    }
    
    // Configure the OpenFlow network domain
    Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper> ();
    Ptr<Controller0> ctrl0 = CreateObject<Controller0> ();

    of13Helper->InstallController (controllers.Get (0), ctrl0);
    for (int i = 0; i < numNodes; i++)
    {
        of13Helper->InstallSwitch (switches.Get(i), switchPorts[i]);
    }
    of13Helper->CreateOpenFlowChannels ();

    // Install the TCP/IP stack into hosts nodes
    InternetStackHelper internet;
    internet.Install (hosts);

    // Set IPv4 host addresses
    Ipv4AddressHelper ipv4helpr;
    Ipv4InterfaceContainer hostIpIfaces;
    ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0");
    hostIpIfaces = ipv4helpr.Assign (hostDevices);


    UdpServerHelper Server (9);
    ApplicationContainer serverApps = Server.Install(hosts.Get(sinkNode));
    serverApps.Start (Seconds (SIMSTART));
    serverApps.Stop (Seconds (SIMSTOP));

    /*
    UdpClientHelper Client(hostIpIfaces.GetAddress(sinkNode),9);
    Client.SetAttribute ("MaxPackets", UintegerValue ((SIMSTOP - SIMSTART) * packet_rate / 4));
    Client.SetAttribute ("Interval", TimeValue (Seconds (1 / (packet_rate / 4))));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));
    */

    UdpClientHelper Client(hostIpIfaces.GetAddress(sinkNode),9);
    Client.SetAttribute ("MaxPackets", UintegerValue (10000));
    Client.SetAttribute ("Interval", TimeValue (Seconds (1 / packet_rate)));
    Client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = Client.Install(hosts.Get(sourceNode));
    clientApps.Start (Seconds (SIMSTART));
    clientApps.Stop (Seconds (SIMSTOP));

    // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
    if (trace)
    {
        of13Helper->EnableOpenFlowPcap ("openflow");
        of13Helper->EnableDatapathStats ("switch-stats");
        csmaHelper.EnablePcap ("switch", switchPorts[0], true);
        csmaHelper.EnablePcap ("switch", switchPorts[1], true);
        csmaHelper.EnablePcap ("switch", switchPorts[2], true);
        csmaHelper.EnablePcap ("host", hostDevices);
    

    // Run the simulation
    Simulator::Stop (Seconds (simTime));
    AnimationInterface anim("my-csma-ver1.xml");
    anim.EnablePacketMetadata();

    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (hosts.Get (i), "Hosts"); // Optional
        anim.UpdateNodeColor (hosts.Get (i), 255, 0, 0); // Optional
    }
    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription (switches.Get (i), "OFS"); // Optional
        anim.UpdateNodeColor (switches.Get (i), 0, 255, 0); // Optional
    }
    anim.UpdateNodeDescription (controllers.Get(0), "OFC"); // Optional
    anim.UpdateNodeColor (controllers.Get(0), 0, 0, 255); // Optional

    Simulator::Schedule (Seconds (1), modify);

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

}

class Controller0 : public OFSwitch13Controller
{
protected:
	void
		HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
		{

			if((int)swtch->GetDpId()==1){
				printf("%d",(int)swtch->GetDpId());
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 apply:output=1");
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=3 in_port=1 apply:output=2");
			}
			if((int)swtch->GetDpId()==2){
				printf("%d",(int)swtch->GetDpId());
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,flags=0x0001,prio=2 in_port=2 write:output=3");
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,flags=0x0001,prio=1 in_port=3 write:output=2");
			}
			if((int)swtch->GetDpId()==3){
				printf("%d",(int)swtch->GetDpId());
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=0 write:output=1");
				DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=0 write:output=2");
			}
		}

		ofl_err HandleFlowRemoved (
		    struct ofl_msg_flow_removed *msg, Ptr<const RemoteSwitch> swtch,
		    uint32_t xid){
			if((int)swtch->GetDpId()==1){
				if(j==1){
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=3 apply:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 apply:output=3");
					j=0;
				}else{
					j=1;
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 apply:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 apply:output=2");
				}
			}
			if((int)swtch->GetDpId()==3){
				if(i==1){
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=3 write:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 write:output=3");
					i=0;
				}else{
					i=1;
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=2 in_port=2 write:output=1");
					DpctlExecute (swtch, "flow-mod cmd=add,table=0,hard=2,flags=0x0001,prio=1 in_port=1 write:output=2");
				}
			}
			return 0;
		}
};
