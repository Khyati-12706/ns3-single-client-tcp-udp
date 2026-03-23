#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// -------- GLOBAL COUNTER --------
int totalSent = 0;

// -------- RECEIVE FUNCTION --------
void ReceivePacket (Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom (from)))
    {
        std::cout << "Server received "
                  << packet->GetSize ()
                  << " bytes at "
                  << Simulator::Now ().GetSeconds ()
                  << " s\n";
    }
}

// -------- SEND FUNCTION --------
void SendPacket (Ptr<Socket> socket, uint32_t pktSize,
                 uint32_t pktCount, Time pktInterval)
{
    if (pktCount > 0)
    {
        socket->Send (Create<Packet> (pktSize));
        totalSent++;

        Simulator::Schedule (pktInterval,
                             &SendPacket,
                             socket,
                             pktSize,
                             pktCount - 1,
                             pktInterval);
    }
    else
    {
        std::cout << "\nTotal packets sent = "
                  << totalSent << std::endl;

        socket->Close ();
    }
}

// -------- MAIN FUNCTION --------
int main ()
{
    Time::SetResolution (Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices = p2p.Install (nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    uint16_t port = 8080;

    // -------- SERVER --------
    Ptr<Socket> serverSocket =
        Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());

    InetSocketAddress local =
        InetSocketAddress (Ipv4Address::GetAny (), port);

    serverSocket->Bind (local);
    serverSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

    // -------- CLIENT --------
    Ptr<Socket> clientSocket =
        Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());

    InetSocketAddress remote =
        InetSocketAddress (interfaces.GetAddress (1), port);

    clientSocket->Connect (remote);

    Simulator::Schedule (Seconds (2.0),
                         &SendPacket,
                         clientSocket,
                         1024,     // packet size
                         10,       // number of packets
                         Seconds (1.0));

    Simulator::Stop (Seconds (15.0));

    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}
