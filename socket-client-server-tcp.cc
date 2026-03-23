#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpClientServer");

// -----------------------------------------------------------
// Called when the SERVER receives data on an accepted socket
// -----------------------------------------------------------
void ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
      std::cout << "[SERVER] Received " << packet->GetSize ()
                << " bytes from " << addr.GetIpv4 ()
                << " at time " << Simulator::Now ().GetSeconds ()
                << " s" << std::endl;
    }
}

// -----------------------------------------------------------
// Called when server accepts a new TCP connection
// -----------------------------------------------------------
bool ConnectionRequest (Ptr<Socket> socket, const Address &from)
{
  std::cout << "[SERVER] Connection request received at "
            << Simulator::Now ().GetSeconds () << " s" << std::endl;
  return true; // accept all connections
}

void NewConnectionCreated (Ptr<Socket> socket, const Address &from)
{
  InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
  std::cout << "[SERVER] New TCP connection established from "
            << addr.GetIpv4 () << " at "
            << Simulator::Now ().GetSeconds () << " s" << std::endl;
  socket->SetRecvCallback (MakeCallback (&ReceivePacket));
}

// -----------------------------------------------------------
// CLIENT: Send packets recursively
// -----------------------------------------------------------
void SendPacket (Ptr<Socket> socket, uint32_t pktSize,
                 uint32_t pktCount, Time pktInterval)
{
  if (pktCount > 0)
    {
      int sent = socket->Send (Create<Packet> (pktSize));
      if (sent > 0)
        {
          std::cout << "[CLIENT] Sent " << pktSize
                    << " bytes at " << Simulator::Now ().GetSeconds ()
                    << " s  (remaining: " << pktCount - 1 << ")" << std::endl;
        }
      Simulator::Schedule (pktInterval, &SendPacket,
                           socket, pktSize, pktCount - 1, pktInterval);
    }
  else
    {
      std::cout << "[CLIENT] All packets sent. Closing socket at "
                << Simulator::Now ().GetSeconds () << " s" << std::endl;
      socket->Close ();
    }
}

// -----------------------------------------------------------
// Called when TCP connection to server is confirmed
// -----------------------------------------------------------
void ConnectionSucceeded (Ptr<Socket> socket)
{
  std::cout << "[CLIENT] TCP Connection established at "
            << Simulator::Now ().GetSeconds () << " s" << std::endl;

  // Start sending 10 packets of 1024 bytes, 1 second apart
  Simulator::Schedule (Seconds (0.0), &SendPacket,
                       socket, 1024, 10, Seconds (1.0));
}

void ConnectionFailed (Ptr<Socket> socket)
{
  std::cout << "[CLIENT] TCP Connection FAILED at "
            << Simulator::Now ().GetSeconds () << " s" << std::endl;
}

// -----------------------------------------------------------
// MAIN
// -----------------------------------------------------------
int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("TcpClientServer", LOG_LEVEL_INFO);

  // ── Create 2 nodes ──────────────────────────────────────
  NodeContainer nodes;
  nodes.Create (2);

  // ── Point-to-point link ──────────────────────────────────
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute  ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay",    StringValue ("2ms"));
  NetDeviceContainer devices = p2p.Install (nodes);

  // ── Internet stack ───────────────────────────────────────
  InternetStackHelper internet;
  internet.Install (nodes);

  // ── IP addresses ─────────────────────────────────────────
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 8080;

  // ── SERVER (node 1) ──────────────────────────────────────
  Ptr<Socket> serverSocket =
    Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());

  InetSocketAddress local =
    InetSocketAddress (Ipv4Address::GetAny (), port);

  serverSocket->Bind (local);
  serverSocket->Listen ();                                      // TCP: must Listen
  serverSocket->SetAcceptCallback (
    MakeCallback (&ConnectionRequest),
    MakeCallback (&NewConnectionCreated));

  // ── CLIENT (node 0) ──────────────────────────────────────
  Ptr<Socket> clientSocket =
    Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

  clientSocket->SetConnectCallback (
    MakeCallback (&ConnectionSucceeded),
    MakeCallback (&ConnectionFailed));

  InetSocketAddress remote =
    InetSocketAddress (interfaces.GetAddress (1), port);

  // Schedule the TCP Connect at t=2s
  Simulator::Schedule (Seconds (2.0),
                       [clientSocket, remote] ()
                       {
                         std::cout << "[CLIENT] Initiating TCP connect at "
                                   << Simulator::Now ().GetSeconds ()
                                   << " s" << std::endl;
                         clientSocket->Connect (remote);
                       });

  // ── Run ──────────────────────────────────────────────────
  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
