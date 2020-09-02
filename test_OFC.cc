//my-openflow-ver6.ccより

//#include "ns3/ofswitch13-module.h"

#include "myCsmaOFC.h"


ofl_err TakumaController::HandlePacketIn(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid){
	enum ofp_packet_in_reason reason = msg->reason;
	auto sw_srcIp = swtch->GetIpv4();

	// PacketInの中身確認
	char *msgStr = ofl_structs_match_to_string((struct ofl_match_header*)msg->match, 0);
	//NS_LOG_INFO(msgStr);
	free(msgStr);

	if(reason == OFPR_NO_MATCH){
		// 宛先macアドレス保存
		Mac48Address ue_dstMac;
		struct ofl_match_tlv *ethDst = oxm_match_lookup(OXM_OF_ETH_DST, (struct ofl_match*)msg->match);
		ue_dstMac.CopyFrom(ethDst->value);
		//NS_LOG_INFO(ue_dstMac);

		// Ethタイプ保存
		// struct ofl_match_tlv *ethType = oxm_match_lookup(OXM_OF_ETH_TYPE, (struct ofl_match*)msg->match);
		// NS_LOG_INFO(ethType->value);

        // 以降の処理がどのスイッチのものか出力
        NS_LOG_INFO(sw_srcIp);

        // 最適経路制御呼び出し
        OptimalPath(sw_srcIp, ue_dstMac);
        // パケット出力
        PacketOut(msg, swtch, xid, opt_db_map[sw_srcIp].flowMap[ue_dstMac].outPort);
        
        // Flow-mod作成
        CreateFlowMod(opt_db_map[sw_srcIp].swtchPtr, ue_dstMac, bl_flag);

        NS_LOG_INFO("-------------------");
	}
	return 0;
}

void TakumaController::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch){
	DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=0 "
		       	"apply:output=ctrl:128");
	DpctlExecute(swtch, "port-desc");

	// COに接続したスイッチの最適経路DB作成(初期化)
	opt_swptr_flowmap_struct opt_emp_entry;
	opt_emp_entry.swtchPtr = swtch;
	std::pair<Ipv4Address, opt_swptr_flowmap_struct> opt_entry (swtch->GetIpv4(), opt_emp_entry);
	opt_db_map.insert(opt_entry);

}

void TakumaController::OptimalPath(Ipv4Address sw_srcIp, Mac48Address ue_dstMac){
	// 最適経路DBを探索
	auto findRoute = opt_db_map[sw_srcIp].flowMap.find(ue_dstMac);
	// 最適経路未登録の場合
	if(findRoute == opt_db_map[sw_srcIp].flowMap.end()){
		// 宛先への最適経路を探索 -> 最適経路テーブル作成
		opt_output_nhop_table path_table;
		// 宛先アドレス別に場合分け
		if(ue_dstMac == "00:00:00:00:00:01"){
			if (sw_srcIp == "10.100.0.2"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (sw_srcIp == "10.100.0.3"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (sw_srcIp == "10.100.0.4"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
			}else if (sw_srcIp == "10.100.0.5"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (sw_srcIp == "10.100.0.6"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (sw_srcIp == "10.100.0.7"){
				NS_LOG_INFO("ポート1へ出力");
				path_table.outPort = 1;
		}else if(ue_dstMac == "00:00:00:00:00:07"){
			if (sw_srcIp == "10.100.0.1"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (sw_srcIp == "10.100.0.2"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (sw_srcIp == "10.100.0.3"){
				NS_LOG_INFO("ポート3へ出力");
				path_table.outPort = 3;
			}else if (sw_srcIp == "10.100.0.4"){
				NS_LOG_INFO("ポート5へ出力");
				path_table.outPort = 5;
			}else if (sw_srcIp == "10.100.0.5"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
			}else if (sw_srcIp == "10.100.0.6"){
				NS_LOG_INFO("ポート2へ出力");
				path_table.outPort = 2;
            }
		}
		// 最適経路DBへ保存
		std::pair<Mac48Address, opt_output_nhop_table> entry (ue_dstMac, path_table);
		opt_db_map[sw_srcIp].flowMap.insert(entry);
	}
}

void TakumaController::CreateFlowMod(Ptr<const RemoteSwitch> swtch, Mac48Address ue_dstMac){
	std::ostringstream cmd;
    cmd << "flow-mod cmd=add,table=0,idle=10,flags=0x0001"
        << ",prio=100" << " eth_dst=" << ue_dstMac
        << " apply:output=" << opt_db_map[swtch->GetIpv4()].flowMap[ue_dstMac].outPort;
    DpctlExecute(swtch, cmd.str());
}

void TakumaController::PacketOut(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid, uint32_t outPort){
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