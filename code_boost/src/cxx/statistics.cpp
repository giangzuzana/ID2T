// Aidmar
#include <iostream>
#include <fstream>
#include <vector>
#include <math.h> 
#include <algorithm>

#include "statistics.h"
#include <sstream>
#include <SQLiteCpp/SQLiteCpp.h>
#include "statistics_db.h"

// Aidmar
// Aidmar
/**
 * Split a string.
 * @param str string to be splitted 
 * @param delimiter delimiter to use in splitting
 * @return vector of substrings
 */
std::vector<std::string> split(std::string str, char delimiter) {
  std::vector<std::string> internal;
  std::stringstream ss(str); // Turn the string into a stream.
  std::string tok;  
  while(getline(ss, tok, delimiter)) {
    internal.push_back(tok);
  }  
  return internal;
}

// Aidmar
/**
 * Get the class (A,B,C,D,E) of IP address.
 * @param ipAddress IP that we get its class
 */
std::string getIPv4Class(std::string ipAddress){
    std::string ipClass="Unknown";
    
    std::vector<std::string> ipBytes = split(ipAddress, '.');
    
    std::cout<< ipAddress << "\n";
    
    if(ipBytes.size()>1){
    int b1 = std::stoi(ipBytes[0]);
    int b2 = std::stoi(ipBytes[1]);
    
    if(b1 >= 1 && b1 <= 126){
        if(b1 == 10)
            ipClass = "A-private";
        else
            ipClass = "A";
    }
    else if(b1 == 127){
        ipClass = "A-unused"; // cannot be used and is reserved for loopback and diagnostic functions.
    }
    else if (b1 >= 128 && b1 <= 191){
        if(b1 == 172 && b2 >= 16 && b2 <= 31) 
            ipClass = "B-private";
        else
            ipClass = "B";
    }
    else if (b1 >= 192 && b1 <= 223){
         if(b1 == 192 && b2 == 168) 
            ipClass = "C-private";
         else
            ipClass = "C";
    }
    else if (b1 >= 224 && b1 <= 239)
        ipClass = "D"; // Reserved for Multicasting
    else if (b1 >= 240 && b1 <= 254)
        ipClass = "E"; // Experimental; used for research    
    }
    /*
     // Could be done by using libtin IPv4Address
    IPv4Range range = IPv4Address("192.168.1.0") / 24;
    range.contains("192.168.1.250"); // Yey, it belongs to this network
    range.contains("192.168.0.100"); // NOPE
    */
    return ipClass;
}

// Aidmar
/**
 * Get closest index for element in vector.
 * @param v vector
 * @param refElem element that we search for or for closest element
 */
int getClosestIndex(std::vector<std::chrono::microseconds> v, std::chrono::microseconds refElem)
{
    auto i = min_element(begin(v), end(v), [=] (std::chrono::microseconds x, std::chrono::microseconds y)
    {
        return std::abs((x - refElem).count()) < std::abs((y - refElem).count());
    });
    return std::distance(begin(v), i);
}


// Aidmar
/**
 * Calculate entropy of source and destination IPs for last time interval and write results to a ip_entropy_interval.csv file.
 * @param intervalStartTimestamp The timstamp where the interval starts.
 */

void statistics::calculateLastIntervalIPsEntropy(std::string filePath, std::chrono::microseconds intervalStartTimestamp){
        std::vector <int> IPsSrcPktsCounts; 
        std::vector <int> IPsDstPktsCounts; 
        
        std::vector <float> IPsSrcProb; 
        std::vector <float> IPsDstProb;
    
        int pktsSent = 0, pktsReceived = 0;
        
        for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
            int indexStartSent = getClosestIndex(i->second.pktsSentTimestamp, intervalStartTimestamp);                         
            int IPsSrcPktsCount = i->second.pktsSentTimestamp.size() - indexStartSent;
            IPsSrcPktsCounts.push_back(IPsSrcPktsCount);
            pktsSent += IPsSrcPktsCount;
            //std::cout<<"IP:"<<i->first<<", indexStartSent:"<<indexStartSent<<", value:"<<i->second.pktsSentTimestamp[indexStartSent].count()<<", IPsSrcPktsCount:"<<IPsSrcPktsCount<<", total_pktsSent:"<<pktsSent<<"\n";
                        
            int indexStartReceived = getClosestIndex(i->second.pktsReceivedTimestamp, intervalStartTimestamp);   
            int IPsDstPktsCount = i->second.pktsReceivedTimestamp.size() - indexStartReceived;       
            IPsDstPktsCounts.push_back(IPsDstPktsCount);
            pktsReceived += IPsDstPktsCount;
        }  
       
         for (auto i = IPsSrcPktsCounts.begin(); i != IPsSrcPktsCounts.end(); i++) {
                IPsSrcProb.push_back((float)*i/pktsSent);
                //std::cout<<"IpSrcProb:"<<(float)*i/pktsSent<<"\n";
         }
         for (auto i = IPsDstPktsCounts.begin(); i != IPsDstPktsCounts.end(); i++) {
                IPsDstProb.push_back((float)*i/pktsReceived);
                //std::cout<<"IpDstProb:"<<(float)*i/pktsReceived<<"\n";
         }
         
         // Calculate IP source entropy 
        float IPsSrcEntropy = 0;
        for(unsigned i=0; i < IPsSrcProb.size();i++){
            if (IPsSrcProb[i] > 0)
                IPsSrcEntropy += - IPsSrcProb[i]*log2(IPsSrcProb[i]);
        }
        // Calculate IP destination entropy
        float IPsDstEntropy = 0;
        for(unsigned i=0; i < IPsDstProb.size();i++){
            if (IPsDstProb[i] > 0)
                IPsDstEntropy += - IPsDstProb[i]*log2(IPsDstProb[i]);
        }
        
        // Replace pcap filename with 'filename_ip_entropy'
        std::string new_filepath = filePath;
        const std::string &newExt = "_ip_entropy_interval.csv";
        std::string::size_type h = new_filepath.rfind('.', new_filepath.length());
        if (h != std::string::npos) {
            new_filepath.replace(h, newExt.length(), newExt);
        } else {
            new_filepath.append(newExt);
        }
    
        // Write stats to file
      std::ofstream file;
      file.open (new_filepath,std::ios_base::app);
      file << intervalStartTimestamp.count() << "," << IPsSrcEntropy << "," << IPsDstEntropy << "\n";
      file.close();         
}


// Aidmar - incomplete
/**
 * Calculate entropy for time intervals. After finishing statistics collecting, this method goes through
 * all stored timestamps and calculate entropy of IP source and destination. 
 * Big time overhead!! better to calculate it on fly, while we are processing packets.
 * @param 
 */
/*
void statistics::calculateIntervalIPsEntropy(std::chrono::microseconds interval){
        std::vector <std::string> IPsSrc; 
        std::vector <std::string> IPsDst; 
        std::vector <int> pkts_sent;
        std::vector <int> pkts_received;
        
        std::vector <float> IPsSrcProb; 
        std::vector <float> IPsDstProb;
        
    time_t t = (timestamp_lastPacket.seconds() - timestamp_firstPacket.seconds());
    time_t ms = (timestamp_lastPacket.microseconds() - timestamp_firstPacket.microseconds());
    
    intervalNum = t/interval;
    
     for(int j=0;j<intervalNum;j++){
        intStart = j*interval;
        intEnd = intStart + interval;             
        for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
            for(int x = 0; x<i->second.pktsSentTimestamp.size();x++){ // could have a prob loop on pktsSent, and inside we have pktsReceived..
                if(i->second.pktsSentTimestamp[x]>intStart && i->second.pktsSentTimestamp[x]<intEnd){
                     IPsSrc.push_back(i->first);   
                }
                if(i->second.pktsReceivedTimestamp[x]>intStart && i->second.pktsReceivedTimestamp[x]<intEnd){
                     IPsDst.push_back(i->first);   
                }
            }                           
        }        
        //IPsSrcProb.push_back((float)i->second.pkts_sent/packetCount);
        //IPsDstProb.push_back((float)i->second.pkts_received/packetCount);
    }      
}*/


// Aidmar
/**
 * Calculate cumulative entropy of source and destination IPs; the entropy for packets from the beginning of the pcap file. 
 * The function write the results to filePath_ip_entropy.csv file.
 * @param filePath The PCAP fiel path.
 */
void statistics::addIPEntropy(std::string filePath){
    std::vector <std::string> IPs; 
    std::vector <float> IPsSrcProb; 
    std::vector <float> IPsDstProb;
    for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
        IPs.push_back(i->first);        
        IPsSrcProb.push_back((float)i->second.pkts_sent/packetCount);
        IPsDstProb.push_back((float)i->second.pkts_received/packetCount);
        
        /*std::cout << i->first << ":" << i->second.pkts_sent << ":" << i->second.pkts_received << ":" 
        << i->second.firstAppearAsSenderPktCount << ":" << i->second.firstAppearAsReceiverPktCount << ":" 
        << packetCount << "\n";*/  
    }
    
    // Calculate IP source entropy 
    float IPsSrcEntropy = 0;
    for(unsigned i=0; i < IPsSrcProb.size();i++){
        if (IPsSrcProb[i] > 0)
            IPsSrcEntropy += - IPsSrcProb[i]*log2(IPsSrcProb[i]);
    }
    std::cout << packetCount << ": SrcEnt: " << IPsSrcEntropy << "\n";
    
    // Calculate IP destination entropy
    float IPsDstEntropy = 0;
    for(unsigned i=0; i < IPsDstProb.size();i++){
        if (IPsDstProb[i] > 0)
            IPsDstEntropy += - IPsDstProb[i]*log2(IPsDstProb[i]);
    }
    std::cout << packetCount << ": DstEnt: " << IPsDstEntropy << "\n";
       
    // Write stats to file
      std::ofstream file;
      
     // Replace pcap filename with 'filename_ip_entropy'
    std::string new_filepath = filePath;
    const std::string &newExt = "_ip_entropy.csv";
    std::string::size_type h = new_filepath.rfind('.', new_filepath.length());
    if (h != std::string::npos) {
        new_filepath.replace(h, newExt.length(), newExt);
    } else {
        new_filepath.append(newExt);
    }
    
    
      file.open (new_filepath,std::ios_base::app);
      file << packetCount << "," << IPsSrcEntropy << "," << IPsDstEntropy << "\n";
      file.close();    
}

// Aidmar
/**
 * Increments the packet counter for the given flow.
 * @param ipAddressSender The sender IP address.
 * @param sport The source port.
 * @param ipAddressReceiver The receiver IP address.
 * @param dport The destination port.
 * @param timestamp The timestamp of the packet.
 */
void statistics::addFlowStat(std::string ipAddressSender,int sport,std::string ipAddressReceiver,int dport, std::chrono::microseconds timestamp){   
    
    
    flow f1 = {ipAddressReceiver, dport, ipAddressSender, sport};
    flow f2 = {ipAddressSender, sport, ipAddressReceiver, dport};
    
    // if already exist A(ipAddressReceiver, dport), B(ipAddressSender, sport)
    if (flow_statistics.count(f1)>0){
        flow_statistics[f1].pkts_B_A++;
        flow_statistics[f1].pkts_B_A_timestamp.push_back(timestamp);
        if(flow_statistics[f1].pkts_A_B_timestamp.size()>0){
            flow_statistics[f1].pkts_delay.push_back(std::chrono::duration_cast<std::chrono::microseconds> (timestamp - flow_statistics[f1].pkts_A_B_timestamp[flow_statistics[f1].pkts_A_B_timestamp.size()-1]));
        }
        
        //std::cout<<timestamp.count()<<"::"<<ipAddressReceiver<<":"<<dport<<","<<ipAddressSender<<":"<<sport<<"\n"; 
        //std::cout<<flow_statistics[f1].pkts_A_B<<"\n";
        //std::cout<<flow_statistics[f1].pkts_B_A<<"\n";
    }
    else{
        flow_statistics[f2].pkts_A_B++;
        flow_statistics[f2].pkts_A_B_timestamp.push_back(timestamp);
         if(flow_statistics[f2].pkts_B_A_timestamp.size()>0){
            flow_statistics[f2].pkts_delay.push_back(std::chrono::duration_cast<std::chrono::microseconds> (timestamp - flow_statistics[f2].pkts_B_A_timestamp[flow_statistics[f2].pkts_B_A_timestamp.size()-1]));
        }
        //std::cout<<timestamp.count()<<"::"<<ipAddressSender<<":"<<sport<<","<<ipAddressReceiver<<":"<<dport<<"\n"; 
        //std::cout<<flow_statistics[f2].pkts_A_B<<"\n";
        //std::cout<<flow_statistics[f2].pkts_B_A<<"\n";
    }        
}
    
    
// Aidmar
/**
 * Increments the packet counter for the given IP address and MSS value.
 * @param ipAddress The IP address whose MSS packet counter should be incremented.
 * @param mssValue The MSS value of the packet.
 */
void statistics::incrementMSScount(std::string ipAddress, int mssValue) {
    mss_distribution[{ipAddress, mssValue}]++;
}

// Aidmar
/**
 * Increments the packet counter for the given IP address and window size.
 * @param ipAddress The IP address whose window size packet counter should be incremented.
 * @param winSize The window size of the packet.
 */
void statistics::incrementWinCount(std::string ipAddress, int winSize) {
    win_distribution[{ipAddress, winSize}]++;
}

/**
 * Increments the packet counter for the given IP address and TTL value.
 * @param ipAddress The IP address whose TTL packet counter should be incremented.
 * @param ttlValue The TTL value of the packet.
 */
void statistics::incrementTTLcount(std::string ipAddress, int ttlValue) {
    ttl_distribution[{ipAddress, ttlValue}]++;
}

/**
 * Increments the protocol counter for the given IP address and protocol.
 * @param ipAddress The IP address whose protocol packet counter should be incremented.
 * @param protocol The protocol of the packet.
 */
void statistics::incrementProtocolCount(std::string ipAddress, std::string protocol) {
    protocol_distribution[{ipAddress, protocol}]++;
}

/**
 * Returns the number of packets seen for the given IP address and protocol.
 * @param ipAddress The IP address whose packet count is wanted.
 * @param protocol The protocol whose packet count is wanted.
 * @return an integer: the number of packets
 */
int statistics::getProtocolCount(std::string ipAddress, std::string protocol) {
    return protocol_distribution[{ipAddress, protocol}];
}

/**
 * Increments the packet counter for
 * - the given sender IP address with outgoing port and
 * - the given receiver IP address with incoming port.
 * @param ipAddressSender The IP address of the packet sender.
 * @param outgoingPort The port used by the sender.
 * @param ipAddressReceiver The IP address of the packet receiver.
 * @param incomingPort The port used by the receiver.
 */
void statistics::incrementPortCount(std::string ipAddressSender, int outgoingPort, std::string ipAddressReceiver,
                                    int incomingPort) {
    ip_ports[{ipAddressSender, "out", outgoingPort}]++;
    ip_ports[{ipAddressReceiver, "in", incomingPort}]++;
}

/**
 * Creates a new statistics object.
 */
statistics::statistics(void) {
}

/**
 * Stores the assignment IP address -> MAC address.
 * @param ipAddress The IP address belonging to the given MAC address.
 * @param macAddress The MAC address belonging to the given IP address.
 */
void statistics::assignMacAddress(std::string ipAddress, std::string macAddress) {
    ip_mac_mapping[ipAddress] = macAddress;
}

/**
 * Registers statistical data for a sent packet. Increments the counter packets_sent for the sender and
 * packets_received for the receiver. Adds the bytes as kbytes_sent (sender) and kybtes_received (receiver).
 * @param ipAddressSender The IP address of the packet sender.
 * @param ipAddressReceiver The IP address of the packet receiver.
 * @param bytesSent The packet's size.
 */
void statistics::addIpStat_packetSent(std::string filePath, std::string ipAddressSender, std::string ipAddressReceiver, long bytesSent, std::chrono::microseconds timestamp) {
    
    // Aidmar - Adding IP as a sender for first time
    if(ip_statistics[ipAddressSender].pkts_sent==0){  
        // Add the IP class
        ip_statistics[ipAddressSender].ip_class = getIPv4Class(ipAddressSender);
        
        // Caculate Mahoney anomaly score for ip.src
        float ipSrc_Mahoney_score = 0;
        // s_r: The number of IP sources (the different values)
        // n: The number of the total instances
        // s_t: The "time" since last anomalous (novel) IP was appeared
        int s_t = 0, n = 0, s_r = 0;        
        for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
                if (i->second.pkts_sent > 0)
                    s_r++;
            }
        if(s_r > 0){
            // The number of the total instances
            n = packetCount;
            // The packet count when the last novel IP was added as a sender
            int pktCntNvlSndr = 0;
            for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
                if (pktCntNvlSndr < i->second.firstAppearAsSenderPktCount)
                    pktCntNvlSndr = i->second.firstAppearAsSenderPktCount;
            }
            // The "time" since last anomalous (novel) IP was appeared
            s_t = packetCount - pktCntNvlSndr + 1;        
            ipSrc_Mahoney_score = (float)s_t*n/s_r;
        }
        
            // Replace pcap filename with 'filename_ip_entropy'
        std::string new_filepath = filePath;
        const std::string &newExt = "_ip_src_anomaly_score.csv";
        std::string::size_type h = new_filepath.rfind('.', new_filepath.length());
        if (h != std::string::npos) {
            new_filepath.replace(h, newExt.length(), newExt);
        } else {
            new_filepath.append(newExt);
        }
        
    // Write stats to file
    std::ofstream file;
    file.open (new_filepath,std::ios_base::app);
    file << ipAddressSender << ","<< s_t << "," << n << "," << s_r << "," << ipSrc_Mahoney_score << "\n";
    file.close();    
    ip_statistics[ipAddressSender].firstAppearAsSenderPktCount = packetCount;  
    ip_statistics[ipAddressSender].sourceAnomalyScore = ipSrc_Mahoney_score;    
    }
    
    // Aidmar - Adding IP as a receiver for first time
    if(ip_statistics[ipAddressReceiver].pkts_received==0){
        // Add the IP class
        ip_statistics[ipAddressReceiver].ip_class = getIPv4Class(ipAddressReceiver); 
        
        // Caculate Mahoney anomaly score for ip.dst
        float ipDst_Mahoney_score = 0;
        // s_r: The number of IP sources (the different values)
        // n: The number of the total instances
        // s_t: The "time" since last anomalous (novel) IP was appeared
        int s_t = 0, n = 0, s_r = 0;        
        for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
                if (i->second.pkts_received > 0)
                    s_r++;
            }
        if(s_r > 0){
            // The number of the total instances
            n = packetCount;
            // The packet count when the last novel IP was added as a sender
            int pktCntNvlRcvr = 0;
            for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
                if (pktCntNvlRcvr < i->second.firstAppearAsReceiverPktCount)
                    pktCntNvlRcvr = i->second.firstAppearAsReceiverPktCount;
            }
            // The "time" since last anomalous (novel) IP was appeared
            s_t = packetCount - pktCntNvlRcvr + 1;
        
            ipDst_Mahoney_score = (float)s_t*n/s_r;
        }
        
        // Replace pcap filename with 'filename_ip_entropy'
        std::string new_filepath = filePath;
        const std::string &newExt = "_ip_dst_anomaly_score.csv";
        std::string::size_type h = new_filepath.rfind('.', new_filepath.length());
        if (h != std::string::npos) {
            new_filepath.replace(h, newExt.length(), newExt);
        } else {
            new_filepath.append(newExt);
        }
        
    // Write stats to file
    std::ofstream file;
    file.open (new_filepath,std::ios_base::app);
    file << ipAddressReceiver << ","<< s_t << "," << n << "," << s_r << "," << ipDst_Mahoney_score << "\n";
    file.close();        
    ip_statistics[ipAddressReceiver].firstAppearAsReceiverPktCount = packetCount;
    ip_statistics[ipAddressReceiver].destinationAnomalyScore = ipDst_Mahoney_score;
    }
    
    // Update stats for packet sender
    ip_statistics[ipAddressSender].kbytes_sent += (float(bytesSent) / 1024);
    ip_statistics[ipAddressSender].pkts_sent++;
    // Aidmar
    ip_statistics[ipAddressSender].pktsSentTimestamp.push_back(timestamp);
    
    // Update stats for packet receiver
    ip_statistics[ipAddressReceiver].kbytes_received += (float(bytesSent) / 1024);
    ip_statistics[ipAddressReceiver].pkts_received++;  
     // Aidmar
    ip_statistics[ipAddressReceiver].pktsReceivedTimestamp.push_back(timestamp);
}

/**
 * Registers a value of the TCP option Maximum Segment Size (MSS).
 * @param ipAddress The IP address which sent the TCP packet.
 * @param MSSvalue The MSS value found.
 */
void statistics::addMSS(std::string ipAddress, int MSSvalue) {
    ip_sumMss[ipAddress] += MSSvalue;
}

/**
 * Setter for the timestamp_firstPacket field.
 * @param ts The timestamp of the first packet in the PCAP file.
 */
void statistics::setTimestampFirstPacket(Tins::Timestamp ts) {
    timestamp_firstPacket = ts;
}

/**
 * Setter for the timestamp_lastPacket field.
 * @param ts The timestamp of the last packet in the PCAP file.
 */
void statistics::setTimestampLastPacket(Tins::Timestamp ts) {
    timestamp_lastPacket = ts;
}

// Aidmar
/**
 * Getter for the timestamp_firstPacket field.
 */
Tins::Timestamp statistics::getTimestampFirstPacket() {
    return timestamp_firstPacket;
}
/**
 * Getter for the timestamp_lastPacket field.
 */
Tins::Timestamp statistics::getTimestampLastPacket() {
    return timestamp_lastPacket;
}

/**
 * Calculates the capture duration.
 * @return a formatted string HH:MM:SS.mmmmmm with
 * HH: hour, MM: minute, SS: second, mmmmmm: microseconds
 */
std::string statistics::getCaptureDurationTimestamp() const {
    // Calculate duration
    time_t t = (timestamp_lastPacket.seconds() - timestamp_firstPacket.seconds());
    time_t ms = (timestamp_lastPacket.microseconds() - timestamp_firstPacket.microseconds());
    long int hour = t / 3600;
    long int remainder = (t - hour * 3600);
    long int minute = remainder / 60;
    long int second = (remainder - minute * 60) % 60;
    long int microseconds = ms;
    // Build desired output format: YYYY-mm-dd hh:mm:ss
    char out[64];
    sprintf(out, "%02ld:%02ld:%02ld.%06ld ", hour, minute, second, microseconds);
    return std::string(out);
}

/**
 * Calculates the capture duration.
 * @return a formatted string SS.mmmmmm with
 * S: seconds (UNIX time), mmmmmm: microseconds
 */
float statistics::getCaptureDurationSeconds() const {
    timeval d;
    d.tv_sec = timestamp_lastPacket.seconds() - timestamp_firstPacket.seconds();
    d.tv_usec = timestamp_lastPacket.microseconds() - timestamp_firstPacket.microseconds();
    char tmbuf[64], buf[64];
    auto nowtm = localtime(&(d.tv_sec));
    strftime(tmbuf, sizeof(tmbuf), "%S", nowtm);
    snprintf(buf, sizeof(buf), "%s.%06u", tmbuf, (uint) d.tv_usec);
    return std::stof(std::string(buf));
}

/**
 * Creates a timestamp based on a time_t seconds (UNIX time format) and microseconds.
 * @param seconds
 * @param microseconds
 * @return a formatted string Y-m-d H:M:S.m with
 * Y: year, m: month, d: day, H: hour, M: minute, S: second, m: microseconds
 */
std::string statistics::getFormattedTimestamp(time_t seconds, suseconds_t microseconds) const {
    timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = microseconds;
    char tmbuf[64], buf[64];
    auto nowtm = localtime(&(tv.tv_sec));
    strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", nowtm);
    snprintf(buf, sizeof(buf), "%s.%06u", tmbuf, (uint) tv.tv_usec);
    return std::string(buf);
}

/**
 * Calculates the statistics for a given IP address.
 * @param ipAddress The IP address whose statistics should be calculated.
 * @return a ip_stats struct containing statistical data derived by the statistical data collected.
 */
ip_stats statistics::getStatsForIP(std::string ipAddress) {
    float duration = getCaptureDurationSeconds();
    entry_ipStat ipStatEntry = ip_statistics[ipAddress];

    ip_stats s;
    s.bandwidthKBitsIn = (ipStatEntry.kbytes_received / duration) * 8;
    s.bandwidthKBitsOut = (ipStatEntry.kbytes_sent / duration) * 8;
    s.packetPerSecondIn = (ipStatEntry.pkts_received / duration);
    s.packetPerSecondOut = (ipStatEntry.pkts_sent / duration);
    s.AvgPacketSizeSent = (ipStatEntry.kbytes_sent / ipStatEntry.pkts_sent);
    s.AvgPacketSizeRecv = (ipStatEntry.kbytes_received / ipStatEntry.pkts_received);
    int sumMSS = ip_sumMss[ipAddress];
    int tcpPacketsSent = getProtocolCount(ipAddress, "TCP");
    s.AvgMaxSegmentSizeTCP = ((sumMSS > 0 && tcpPacketsSent > 0) ? (sumMSS / tcpPacketsSent) : 0);

    return s;
}

/**
 * Increments the packet counter.
 */
void statistics::incrementPacketCount() {
    packetCount++;
}

/**
 * Prints the statistics of the PCAP and IP specific statistics for the given IP address.
 * @param ipAddress The IP address whose statistics should be printed. Can be empty "" to print only general file statistics.
 */
void statistics::printStats(std::string ipAddress) {
    std::stringstream ss;
    ss << std::endl;
    ss << "Capture duration: " << getCaptureDurationSeconds() << " seconds" << std::endl;
    ss << "Capture duration (HH:MM:SS.mmmmmm): " << getCaptureDurationTimestamp() << std::endl;
    ss << "#Packets: " << packetCount << std::endl;
    ss << std::endl;

    // Print IP address specific statistics only if IP address was given
    if (ipAddress != "") {
        entry_ipStat e = ip_statistics[ipAddress];
        ss << "\n----- STATS FOR IP ADDRESS [" << ipAddress << "] -------" << std::endl;
        ss << std::endl << "KBytes sent: " << e.kbytes_sent << std::endl;
        ss << "KBytes received: " << e.kbytes_received << std::endl;
        ss << "Packets sent: " << e.pkts_sent << std::endl;
        ss << "Packets received: " << e.pkts_received << "\n\n";

        ip_stats is = getStatsForIP(ipAddress);
        ss << "Bandwidth IN: " << is.bandwidthKBitsIn << " kbit/s" << std::endl;
        ss << "Bandwidth OUT: " << is.bandwidthKBitsOut << " kbit/s" << std::endl;
        ss << "Packets per second IN: " << is.packetPerSecondIn << std::endl;
        ss << "Packets per second OUT: " << is.packetPerSecondOut << std::endl;
        ss << "Avg Packet Size Sent: " << is.AvgPacketSizeSent << " kbytes" << std::endl;
        ss << "Avg Packet Size Received: " << is.AvgPacketSizeRecv << " kbytes" << std::endl;
        ss << "Avg MSS: " << is.AvgMaxSegmentSizeTCP << " bytes" << std::endl;
    }
    std::cout << ss.str();
}

/**
 * Derives general PCAP file statistics from the collected statistical data and
 * writes all data into a SQLite database, located at database_path.
 * @param database_path The path of the SQLite database file ending with .sqlite3.
 */
void statistics::writeToDatabase(std::string database_path) {
    // Generate general file statistics
    float duration = getCaptureDurationSeconds();
    long sumPacketsSent = 0, senderCountIP = 0;
    float sumBandwidthIn = 0.0, sumBandwidthOut = 0.0;
    for (auto i = ip_statistics.begin(); i != ip_statistics.end(); i++) {
        sumPacketsSent += i->second.pkts_sent;
        // Consumed bandwith (bytes) for sending packets
        sumBandwidthIn += (i->second.kbytes_received / duration);
        sumBandwidthOut += (i->second.kbytes_sent / duration);
        senderCountIP++;
    }

    float avgPacketRate = (packetCount / duration);
    long avgPacketSize = getAvgPacketSize();
    long avgPacketsSentPerHost = (sumPacketsSent / senderCountIP);
    float avgBandwidthInKBits = (sumBandwidthIn / senderCountIP) * 8;
    float avgBandwidthOutInKBits = (sumBandwidthOut / senderCountIP) * 8;

    // Create database and write information
    statistics_db db(database_path);
    db.writeStatisticsFile(packetCount, getCaptureDurationSeconds(),
                           getFormattedTimestamp(timestamp_firstPacket.seconds(), timestamp_firstPacket.microseconds()),
                           getFormattedTimestamp(timestamp_lastPacket.seconds(), timestamp_lastPacket.microseconds()),
                           avgPacketRate, avgPacketSize, avgPacketsSentPerHost, avgBandwidthInKBits,
                           avgBandwidthOutInKBits);
    db.writeStatisticsIP(ip_statistics);
    db.writeStatisticsTTL(ttl_distribution);
    db.writeStatisticsIpMac(ip_mac_mapping);
    db.writeStatisticsMss(ip_sumMss);
    db.writeStatisticsPorts(ip_ports);
    db.writeStatisticsProtocols(protocol_distribution);
    // Aidmar
    db.writeStatisticsMss_dist(mss_distribution);
    db.writeStatisticsWin(win_distribution);
    db.writeStatisticsFlow(flow_statistics);
}

/**
 * Returns the average packet size.
 * @return a float indicating the average packet size in kbytes.
 */
float statistics::getAvgPacketSize() const {
    // AvgPktSize = (Sum of all packet sizes / #Packets)
    return (sumPacketSize / packetCount) / 1024;
}

/**
 * Adds the size of a packet (to be used to calculate the avg. packet size).
 * @param packetSize The size of the current packet in bytes.
 */
void statistics::addPacketSize(uint32_t packetSize) {
    sumPacketSize += ((float) packetSize);
}






































