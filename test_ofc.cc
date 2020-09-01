#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-apps-module.h>
#include <ns3/netanim-module.h>
#include <ns3/applications-module.h>
#include <ns3/udp-header.h>
#include <ns3/mobility-module.h>
#include <ns3/flow-monitor-helper.h>
#include <ns3/flow-monitor-module.h>
#include <regex>

#define PORT		30

#define PROG_DIR	"/media/sf_ns-3_ubuntu18.04/kakushin/openflow_routing/"

using namespace ns3;

class KakushinController;

NS_LOG_COMPONENT_DEFINE ("kakushin_openflow_routing");

int
main (int argc, char *argv[])
{
	double time = 12.0;
    double start = 1.0;
    double stop = 11.0;
    Time simTime = Seconds (time);
    Time simStart = Seconds (start);
    Time simStop = Seconds (stop);
	bool verbose = false;
	bool trace = false;
	bool bl_cost = false;
	double packet_rate = 100;

	// Configure command line parameters
	CommandLine cmd;
	cmd.AddValue ("verbose", "Enable verbose output", verbose);
	cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
	cmd.AddValue ("bl_cost", "true: bl is equal, false: bl is different", bl_cost);
	cmd.AddValue ("packet_rate", "packet occurrence rate (??? packet/s)", packet_rate);
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

	// Enable checksum computations (required by OFSwitch13 module)
	GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

	// 初期ノード設定
	int acc_max_nw = 4;
	int acc_max_node = 4;
	int cor_max_nw = 2;
	int cor_max_node = 4;
	int exc_max_nw = 1; 
	int exc_max_node = 6;
	int cor_exc_nodes_num = (cor_max_node/2) * cor_max_nw; //コア層-対外層のノード数
	// ノード作成
	// アクセス層
	NodeContainer acc_nw_nodes[acc_max_nw];
	for (int i = 0; i < acc_max_nw; i++){
		acc_nw_nodes[i].Create(acc_max_node);
	}
	// コア層
	NodeContainer cor_nw_nodes[cor_max_nw];
	int j = 0;
	for (int i = 0; i < cor_max_nw; i++){
		cor_nw_nodes[i].Add(acc_nw_nodes[j].Get(acc_max_node - 1));
		cor_nw_nodes[i].Add(acc_nw_nodes[j+1].Get(acc_max_node - 1));
		cor_nw_nodes[i].Create(cor_max_node - 2);
		j += 2;
	}
	// 対外層
	NodeContainer exc_nw_nodes[exc_max_nw];
	for (int i = 0; i < exc_max_nw; i++){
		exc_nw_nodes[i].Add(cor_nw_nodes[i].Get(acc_max_node - 2));
		exc_nw_nodes[i].Add(cor_nw_nodes[i].Get(acc_max_node - 1));
		exc_nw_nodes[i].Add(cor_nw_nodes[i+1].Get(acc_max_node - 2));
		exc_nw_nodes[i].Add(cor_nw_nodes[i+1].Get(acc_max_node - 1));
		exc_nw_nodes[i].Create(exc_max_node - 4);
	}
	// 10Mbps
	CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
	csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

	// NetDevice作成
	// 下準備(スイッチごとのポート管理配列)
	NodeContainer pair;
	NetDeviceContainer pairDevs;
	NetDeviceContainer hostDevices;
	// openflow設定で使用
	NetDeviceContainer ofRouterPorts[10];
	NodeContainer ofRouters;
	NodeContainer ueNodes;
	for (int i = 0; i < 10; i++){
		ofRouterPorts[i] = NetDeviceContainer ();
	}
	
	// アクセス層
	for (int i = 0; i < acc_max_nw; i++){
			for (int j = 0; j < acc_max_node - 1; j++){
				pair = NodeContainer (acc_nw_nodes[i].Get(j), acc_nw_nodes[i].Get(acc_max_node - 1));
				pairDevs = csma.Install (pair);
				// ホスト格納
				hostDevices.Add (pairDevs.Get(0));
				ueNodes.Add (acc_nw_nodes[i].Get(j));
				// OFスイッチのポート格納
				ofRouterPorts[i].Add (pairDevs.Get(1));
			}
			// OFスイッチ格納
			ofRouters.Add (acc_nw_nodes[i].Get(acc_max_node - 1));
	}
	// コア層
	for (int i = 0; i < cor_max_nw; i++){
		for (int j = 0; j < cor_max_node/2; j++){
			for (int k = cor_max_node/2; k < cor_max_node; k++){
				pair = NodeContainer (cor_nw_nodes[i].Get(j), cor_nw_nodes[i].Get(k));

				// bl_cost: true => BL帯域幅は等しい, false => BL帯域幅は異なる
				if(i == 0) {
					if (j==0 && k==cor_max_node/2){
						pairDevs = csma.Install (pair);
					}else{
						pairDevs = csma.Install (pair);
					}
				}else if(i == 1 && !bl_cost){
					if (j==(cor_max_node/2 - 1) && k==cor_max_node/2){
						pairDevs = csma.Install (pair);
					}else{
						pairDevs = csma.Install (pair);
					}
				}else{
					pairDevs = csma.Install (pair);
				}

				ofRouterPorts[j+(i*(cor_max_node/2))].Add (pairDevs.Get(0));
				ofRouterPorts[k+((i+1)*(cor_max_node/2))].Add (pairDevs.Get(1));
				if (j==0){
					ofRouters.Add (cor_nw_nodes[i].Get(k));
				}
			}
		}
	}
	// 対外層
	for (int i = 0; i < exc_max_nw; i++){
		for (int j = 0; j < cor_exc_nodes_num; j++){
			for (int k = cor_exc_nodes_num; k < exc_max_node; k++){
				pair = NodeContainer (exc_nw_nodes[i].Get(j), exc_nw_nodes[i].Get(k));
				if ((j == 0 && k == cor_exc_nodes_num) || (j == 2 && k == cor_exc_nodes_num)){
					pairDevs = csma.Install (pair);
				}else{
					pairDevs = csma.Install (pair);
				}
				ofRouterPorts[j+cor_exc_nodes_num].Add (pairDevs.Get(0));
				ofRouterPorts[k+cor_exc_nodes_num].Add (pairDevs.Get(1));
				if (j==0){
					ofRouters.Add (exc_nw_nodes[i].Get(k));
				}
			}
		}
	}

	// openflow設定
	// コントローラノード作成
	Ptr<Node> controllerNode = CreateObject<Node> ();
	//設定
	Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper> ();
	Ptr<KakushinController> ctrl = CreateObject<KakushinController> ();
	of13Helper->InstallController (controllerNode, ctrl);
	for (int i = 0; i < 10; i++){
		of13Helper->InstallSwitch (ofRouters.Get (i), ofRouterPorts [i]);
	}
	of13Helper->CreateOpenFlowChannels ();

	// プロトコルスタックインストール
	InternetStackHelper internet;
	internet.Install (ueNodes);

	// アクセス層の一部にip割り当て
	Ipv4AddressHelper ipv4helpr;
	Ipv4InterfaceContainer hostIpIfaces;
	ipv4helpr.SetBase ("10.1.1.0", "255.255.255.0");
	hostIpIfaces = ipv4helpr.Assign (hostDevices);

	UdpServerHelper Server1 (PORT);
    ApplicationContainer serverApps1 = Server1.Install(ueNodes.Get(4));
    serverApps1.Start (simStart);
    serverApps1.Stop (simStop);

    UdpClientHelper client(hostIpIfaces.GetAddress(4),PORT);
	client.SetAttribute("MaxPackets", UintegerValue((stop - start)*(packet_rate/4)));
	client.SetAttribute("Interval", TimeValue(Seconds(1/(packet_rate/4))));
	client.SetAttribute("PacketSize", UintegerValue(1460));

    ApplicationContainer clientApps1 = client.Install (ueNodes.Get (8));
    clientApps1.Start (simStart);
    clientApps1.Stop (simStop);

	// Enable datapath stats and pcap traces at ueNodes, switch(es), and controller(s)
	if (trace)
	{
		std::string pfname_co = std::string(PROG_DIR) + "openflow";
		std::string pfname_swtch = std::string(PROG_DIR) + "switch-stats";
		of13Helper->EnableOpenFlowPcap (pfname_co);
		of13Helper->EnableDatapathStats (pfname_swtch);
		//csmaHelper.EnablePcap ("switch", switchPorts [0], true);
		//csmaHelper.EnablePcap ("switch", switchPorts [1], true);
		//csmaHelper.EnablePcap ("host", hostDevices);
	}

	// 初期配置ができるよう,mobilityを各ノードにインストール
	MobilityHelper mobility;
	mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
	"MinX", DoubleValue (0.0),
	"MinY", DoubleValue (0.0),
	"DeltaX", DoubleValue (5.0),
	"DeltaY", DoubleValue (10.0),
	"GridWidth", UintegerValue (3),
	"LayoutType", StringValue ("RowFirst"));
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install(ofRouters);
	mobility.Install(ueNodes);
	mobility.Install(controllerNode);

	// アニメーション
	AnimationInterface anim ("test_ofc.xml");

	// ノードの位置,大きさ設定
	int x = 0;
	int y = 200;
	int x_size = 4.0;
	int y_size = 4.0;
	// アクセス層
	for(int i = 0; i < acc_max_nw; i++){
			for(int j = 0; j < acc_max_node -1; j++){
				anim.SetConstantPosition(acc_nw_nodes[i].Get(j), x, y);
				anim.UpdateNodeSize(acc_nw_nodes[i].Get(j)->GetId(), x_size, y_size);
				anim.UpdateNodeColor(acc_nw_nodes[i].Get(j)->GetId(), 0, 255, 0);
				anim.UpdateNodeDescription (acc_nw_nodes[i].Get(j), "access1");
				x = x + 10;
			}
			anim.SetConstantPosition(acc_nw_nodes[i].Get(acc_max_node - 1), x - 20, y - 50);
			anim.UpdateNodeSize(acc_nw_nodes[i].Get(acc_max_node - 1)->GetId(), x_size, y_size);
			anim.UpdateNodeDescription (acc_nw_nodes[i].Get(acc_max_node - 1), "access2");
			x = x + 10;
	}
	// コア層
	x = 10;
	for(int i = 0; i < cor_max_nw; i++){
		for(int j = cor_max_node/2; j < cor_max_node; j++){
			anim.SetConstantPosition(cor_nw_nodes[i].Get(j), x, y - 100);
			anim.UpdateNodeSize(cor_nw_nodes[i].Get(j)->GetId(), x_size, y_size);
			anim.UpdateNodeDescription (cor_nw_nodes[i].Get(j), "core");
			x = x + 40;
		}
	}
	// 対外層
	x = 50;
	for(int i = 0; i < exc_max_nw; i++){
		for(int j = cor_exc_nodes_num; j < exc_max_node; j++){
			anim.SetConstantPosition(exc_nw_nodes[i].Get(j), x, y - 150);
			anim.UpdateNodeSize(exc_nw_nodes[i].Get(j)->GetId(), x_size, y_size);
			anim.UpdateNodeDescription (exc_nw_nodes[i].Get(j), "taigai");
			x = x + 40;
		}
	}
	// OFController
	anim.SetConstantPosition(controllerNode, 70, y - 200);
	anim.UpdateNodeSize(controllerNode->GetId(), x_size, y_size);
	anim.UpdateNodeColor(controllerNode->GetId(), 0, 255, 255);

	// スループット計測準備
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll();

	// Run the simulation
	Simulator::Stop (simTime);
	Simulator::Run ();

	// Print per flow statistics
	monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
	{
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
		std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
		std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
		std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
		std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";
		std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
		std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
		std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (stop - start) / 1000 / 1000  << " Mbps\n";
	}

	Simulator::Destroy ();
}

class KakushinController : public OFSwitch13Controller{
	public:
		ofl_err HandlePacketIn(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid);
		
		void OptimalPath(Ipv4Address t_swtch, Mac48Address dst48);
		uint32_t BroadCastPath(struct ofl_msg_packet_in *msg, Ipv4Address t_swtch);
		void CreateFlowMod(Ptr<const RemoteSwitch> swtch, Mac48Address dst48);
		void PacketOut(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid, uint32_t outPort);

	protected:
		void HandshakeSuccessful(Ptr<const RemoteSwitch> swtch);
	
	private:
		// 最適経路DBデータ構造作成
		// [次ホップ,出力ポート]の構造体
		typedef struct{
			uint32_t outPort;
			Ipv4Address nextHop;
		} opt_output_nhop_table;
		// [スイッチPtr,{ 宛先 : テーブル(出力ポート,ネクストホップ) }]の構造体
		typedef struct{
			Ptr<const RemoteSwitch> swtchPtr;
			std::map<Mac48Address, opt_output_nhop_table> flowMap;
		} opt_swptr_flowmap_struct;
		// { スイッチIP : 構造体(スイッチPtr,フローエントリ群) } のmap
		typedef std::map<Ipv4Address, opt_swptr_flowmap_struct> OPT_DB_MAP;
		// 最適経路DB(mapとして保存されている)
		OPT_DB_MAP opt_db_map;

};

ofl_err KakushinController::HandlePacketIn(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid){
	enum ofp_packet_in_reason reason = msg->reason;
	auto t_swtch = swtch->GetIpv4();

	// PacketInの中身確認
	char *msgStr = ofl_structs_match_to_string((struct ofl_match_header*)msg->match, 0);
	//NS_LOG_INFO(msgStr);
	free(msgStr);

	if(reason == OFPR_NO_MATCH){
		// 宛先macアドレス保存
		Mac48Address dst48;
		struct ofl_match_tlv *ethDst = oxm_match_lookup(OXM_OF_ETH_DST, (struct ofl_match*)msg->match);
		dst48.CopyFrom(ethDst->value);
		//NS_LOG_INFO(dst48);

		// Ethタイプ保存
		// struct ofl_match_tlv *ethType = oxm_match_lookup(OXM_OF_ETH_TYPE, (struct ofl_match*)msg->match);
		// NS_LOG_INFO(ethType->value);

		// ブロードキャスト処理
		if(dst48 == "ff:ff:ff:ff:ff:ff"){
			// 出力ポート検索
			uint32_t outPort = BroadCastPath(msg, t_swtch);
			// パケット出力
			PacketOut(msg, swtch, xid, outPort);
			NS_LOG_INFO("********************");
		}else{
			// 以降の処理がどのスイッチのものか出力
			NS_LOG_INFO(t_swtch);

			// 最適経路制御呼び出し
			OptimalPath(t_swtch, dst48);
			// パケット出力
			PacketOut(msg, swtch, xid, opt_db_map[t_swtch].flowMap[dst48].outPort);

			// Flow-mod作成
			CreateFlowMod(opt_db_map[t_swtch].swtchPtr, dst48);

			NS_LOG_INFO("-------------------");
		}
	}
	return 0;
}

void KakushinController::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch){
	DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=0 "
		       	"apply:output=ctrl:128");
	DpctlExecute(swtch, "port-desc");

	// COに接続したスイッチの最適経路DB作成(初期化)
	opt_swptr_flowmap_struct opt_emp_entry;
	opt_emp_entry.swtchPtr = swtch;
	std::pair<Ipv4Address, opt_swptr_flowmap_struct> opt_entry (swtch->GetIpv4(), opt_emp_entry);
	opt_db_map.insert(opt_entry);

}

void KakushinController::OptimalPath(Ipv4Address t_swtch, Mac48Address dst48){
	// 最適経路DBを探索
	auto findRoute = opt_db_map[t_swtch].flowMap.find(dst48);
	// 最適経路未登録の場合
	if(findRoute == opt_db_map[t_swtch].flowMap.end()){
		// 宛先への最適経路を探索 -> 最適経路テーブル作成
		opt_output_nhop_table path_table;
		// 宛先アドレス別に場合分け
		if(dst48 == "00:00:00:00:00:01"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}
		}else if(dst48 == "00:00:00:00:00:03"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}
		}else if(dst48 == "00:00:00:00:00:05"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}
		}else if(dst48 == "00:00:00:00:00:07"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}
		}else if(dst48 == "00:00:00:00:00:09"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}
		}else if(dst48 == "00:00:00:00:00:0b"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}
		}else if(dst48 == "00:00:00:00:00:0d"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}
		}else if(dst48 == "00:00:00:00:00:0f"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}
		}else if(dst48 == "00:00:00:00:00:11"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}
		}else if(dst48 == "00:00:00:00:00:13"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}
		}else if(dst48 == "00:00:00:00:00:15"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}
		}else if(dst48 == "00:00:00:00:00:17"){
			if (t_swtch == "10.100.0.2"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.3"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.4"){
				NS_LOG_INFO("ポート4へ出力");
				path_table.outPort = 4;
			}else if (t_swtch == "10.100.0.5"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.6"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.7"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.8"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.9"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (t_swtch == "10.100.0.10"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (t_swtch == "10.100.0.11"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}
		}
		// 最適経路DBへ保存
		std::pair<Mac48Address, opt_output_nhop_table> entry (dst48, path_table);
		opt_db_map[t_swtch].flowMap.insert(entry);
	}
}

uint32_t KakushinController::BroadCastPath(struct ofl_msg_packet_in *msg, Ipv4Address t_swtch){
	// 宛先IP保存
	char *msgStr = ofl_structs_match_to_string((struct ofl_match_header*)msg->match, 0);

	// 正規表現での抽出の準備
	std::regex re("arp_tpa=\".*\", arp_sha");
	std::cmatch m;

	// 宛先IP格納先
	std::string dst_ip;

	// 正規表現での抽出処理
	if(std::regex_search(msgStr, m, re)){
		dst_ip = m.str();
	}
	size_t c = dst_ip.find("\", arp_sha");
	dst_ip.erase(c);
	dst_ip.erase(0, 9);
	//NS_LOG_INFO(dst_ip);
	free(msgStr);

	// 宛先別出力ポート探索
	uint32_t outPort;
	if(dst_ip == "10.1.1.1"){
		if(t_swtch == "10.100.0.2"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 1;
		}
	}else if(dst_ip == "10.1.1.2"){
		if(t_swtch == "10.100.0.2"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 1;
		}
	}else if(dst_ip == "10.1.1.3"){
		if(t_swtch == "10.100.0.2"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 1;
		}
	}else if(dst_ip == "10.1.1.4"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 1;
		}
	}else if(dst_ip == "10.1.1.5"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 1;
		}
	}else if(dst_ip == "10.1.1.6"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 1;
		}
	}else if(dst_ip == "10.1.1.7"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 3;
		}
	}else if(dst_ip == "10.1.1.8"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 3;
		}
	}else if(dst_ip == "10.1.1.9"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 3;
		}
	}else if(dst_ip == "10.1.1.10"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 1;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 3;
		}
	}else if(dst_ip == "10.1.1.11"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 3;
		}
	}else if(dst_ip == "10.1.1.12"){
		if(t_swtch == "10.100.0.2"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.3"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.4"){
			outPort = 4;
		}else if(t_swtch == "10.100.0.5"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.6"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.7"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.8"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.9"){
			outPort = 2;
		}else if(t_swtch == "10.100.0.10"){
			outPort = 3;
		}else if(t_swtch == "10.100.0.11"){
			outPort = 3;
		}
	}
	return outPort;
}

void KakushinController::CreateFlowMod(Ptr<const RemoteSwitch> swtch, Mac48Address dst48){
	std::ostringstream cmd;
	cmd << "flow-mod cmd=add,table=0,idle=10,flags=0x0001"
		<< ",prio=100" << " eth_dst=" << dst48
		<< " apply:output=" << opt_db_map[swtch->GetIpv4()].flowMap[dst48].outPort;
	DpctlExecute(swtch, cmd.str());
}

void KakushinController::PacketOut(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid, uint32_t outPort){
	// PacketIn時の入力ポート保存
	uint32_t inPort;
        size_t portLen = OXM_LENGTH (OXM_OF_IN_PORT); // (Always 4 bytes)
        struct ofl_match_tlv *input = oxm_match_lookup (OXM_OF_IN_PORT, (struct ofl_match*)msg->match);
        memcpy (&inPort, input->value, portLen);	

	// PacketOutの準備
	struct ofl_msg_packet_out reply;
	reply.header.type = OFPT_PACKET_OUT;
	reply.buffer_id = msg->buffer_id;
	reply.in_port = inPort;
	reply.data_length = 0;
        reply.data = 0;

	if (msg->buffer_id == NO_BUFFER){
		// No packet buffer. Send data back to switch
		reply.data_length = msg->data_length;
		reply.data = msg->data;
	}
	// PacketOutの実行
	struct ofl_action_output *a = (struct ofl_action_output*)xmalloc (sizeof (struct ofl_action_output));
	a->header.type = OFPAT_OUTPUT;
	a->port = outPort;
	a->max_len = 0;
	reply.actions_num = 1;
	reply.actions = (struct ofl_action_header**)&a;

	SendToSwitch (swtch, (struct ofl_msg_header*)&reply, xid);
	free (a);
}