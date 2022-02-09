#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "ns3/applications-module.h"
#include "ns3/channel-list.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
// #include "ns3/point-to-point-channel.h"

#define CSMA_NUM 170
#define LIV_TIME 10.0
#define CH_DELAY 300
#define EXPONENT 70.0
#define PCK_SIZE 1500

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("App");

using generator = std::default_random_engine;
using distribution = std::exponential_distribution<double>;

uint64_t pack_sent = 0;
uint64_t pack_droped = 0;
uint64_t queue_sum[CSMA_NUM];
uint64_t pack_send[CSMA_NUM];
uint64_t max_queue = 0;

class DistributedApp : public Application {
    virtual void StartApplication();
    virtual void StopApplication();

    void shedule_next_packet();
    void send_packet();

    Ptr<Socket> sock;
    Address addr;
    uint32_t pack_sz;
    EventId send_event;
    bool runnig;
    uint32_t packets_sent;
    generator *generator;
    distribution *distribution;
    Ptr<Queue<Packet>> queue;
    std::string name;
    int m_num;

   public:
    DistributedApp();
    virtual ~DistributedApp();
    void Init(Ptr<Socket> sock_, Address addr_, uint32_t pack_sz_,
              generator *gen_, distribution *distr_, Ptr<Queue<Packet>> que_,
              std::string name_, int num_);
};

void DistributedApp::StartApplication() {
    runnig = true;
    packets_sent = 0;
    sock->Bind();
    sock->Connect(addr);
    sock->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    sock->SetAllowBroadcast(true);
    send_event =
        Simulator::Schedule(Seconds(0.0), &DistributedApp::send_packet, this);
}

void DistributedApp::StopApplication() {
    runnig = false;
    if (send_event.IsRunning()) Simulator::Cancel(send_event);
    if (sock) sock->Close();
}

void DistributedApp::send_packet() {
    Ptr<Packet> packet = Create<Packet>(pack_sz);
    sock->Send(packet);
    pack_sent++;
    shedule_next_packet();
}

void DistributedApp::shedule_next_packet() {
    if (runnig) {
        Time Next(MilliSeconds(1000 * (*distribution)(*generator)));
        queue_sum[m_num] += queue->GetNPackets();
        pack_send[m_num]++;
        if (queue->GetNPackets() > max_queue) max_queue = queue->GetNPackets();
        send_event =
            Simulator::Schedule(Next, &DistributedApp::send_packet, this);
    }
}

DistributedApp::DistributedApp()
    : sock(0),
      addr(),
      pack_sz(0),
      send_event(),
      runnig(false),
      packets_sent(0) {}

DistributedApp::~DistributedApp() {
    sock = 0;
    delete generator;
    delete distribution;
}

void DistributedApp::Init(Ptr<Socket> sock_, Address addr_, uint32_t pack_sz_,
                          generator *gen_, distribution *distr_,
                          Ptr<Queue<Packet>> que_, std::string name_,
                          int num_) {
    sock = sock_;
    addr = addr_;
    pack_sz = pack_sz_;
    generator = gen_;
    distribution = distr_;
    queue = que_;
    name = name_;
    m_num = num_;
}

static void droped_inc(std::string context, Ptr<const Packet> p) {
    pack_droped++;
}

std::vector<bool> business_measurements;

static void tick_business() {
    Ptr<Channel> channel = ChannelList::GetChannel(0);
    Ptr<CsmaChannel> csma_channel = DynamicCast<CsmaChannel>(channel);
    business_measurements.push_back(csma_channel->IsBusy());
    Simulator::Schedule(MilliSeconds(10), &tick_business);
}

int main(int argc, char **argv) {
    LogComponentEnable("App", LOG_LEVEL_INFO);
    NodeContainer nodes;
    nodes.Create(CSMA_NUM);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(CH_DELAY)));
    csma.SetQueue("ns3::DropTailQueue");

    NetDeviceContainer devices = csma.Install(nodes);

    std::vector<Ptr<Queue<Packet>>> queues;
    for (uint32_t i = 0; i < CSMA_NUM; i++) {
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(0.0000000001));
        Ptr<DropTailQueue<Packet>> que = CreateObject<DropTailQueue<Packet>>();
        que->SetMaxSize(QueueSize("50p"));
        que->TraceConnect("Drop", "Host " + std::to_string(i) + ": ",
                          MakeCallback(&droped_inc));
        queues.push_back(que);
        devices.Get(i)->SetAttribute("TxQueue", PointerValue(que));
        devices.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("12.21.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sink_port = 8080;
    Address sinkAddress(
        InetSocketAddress(interfaces.GetAddress(CSMA_NUM - 1), sink_port));
    PacketSinkHelper packetSinkHelper(
        "ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), sink_port));
    ApplicationContainer sinkApps =
        packetSinkHelper.Install(nodes.Get(CSMA_NUM - 1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(LIV_TIME));

    for (uint32_t i = 0; i < CSMA_NUM - 1; i++) {
        Ptr<Socket> ns3UdpSocket =
            Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        Ptr<DistributedApp> app = CreateObject<DistributedApp>();
        app->Init(ns3UdpSocket, sinkAddress, PCK_SIZE, new generator(i),
                  new distribution(EXPONENT), queues[i],
                  "Host " + std::to_string(i), i);
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(LIV_TIME));
    }

    Simulator::Schedule(MilliSeconds(10), &tick_business);

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    double aver_queue = 0;
    for (int t = 0; t < CSMA_NUM - 1; t++)
        aver_queue += double(queue_sum[t]) / double(pack_send[t]);

    int true_cnt = 0;
    for (bool meas : business_measurements) {
        if (meas) ++true_cnt;
    }

    std::cout << "pkts_snd:\t" << pack_sent << std::endl
              << "queue_avg:\t" << aver_queue / double(CSMA_NUM - 1)
              << std::endl
              << "queue_max:\t" << max_queue << std::endl
              << "business_max:\t" << true_cnt / business_measurements.size()
              << std::endl;
    return 0;
}