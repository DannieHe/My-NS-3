#include "ns3/ofswitch13-module.h"

class TakumaController : public OFSwitch13Controller
{
    public:
        ofl_err HandlePacketIn(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid);
		
		void OptimalPath(Ipv4Address sw_srcIp, Mac48Address ue_dstMac);
        void CreateFlowMod(Ptr<const RemoteSwitch> swtch, Mac48Address ue_dstMac);
		void PacketOut(struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid, uint32_t outPort);

    protected:
        // void HandshakeSuccessful (Ptr<const RemoteSwitch> swtch);
        // ofl_err HandleFlowRemoved (struct ofl_msg_flow_removed *msg, Ptr<const RemoteSwitch> swtch, uint32_t xid);
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